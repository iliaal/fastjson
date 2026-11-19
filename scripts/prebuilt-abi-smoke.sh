#!/usr/bin/env bash
# Usage: prebuilt-abi-smoke.sh MODULE.so OLDEST_IMAGE NEWEST_IMAGE
#
# Gates a prebuilt Linux module built against the newest patch release of
# its PHP minor (NEWEST_IMAGE) and checks it against the oldest one. Each image is a digest-pinned official php:X.Y.Z-cli image,
# or a fastjson-local/php:X.Y.Z-cli image the release workflow built in the
# same job from a checksum-pinned php.net tarball (no NTS official image
# exists for that minor); the X.Y.Z in its tag is the PHP_VERSION the image
# must report. Every image must be a non-thread-safe PHP.
#
# 1. Every undefined (non-weak) dynamic symbol of the module must be exported
#    by the newest image's php binary or by a library the module itself links
#    against, as resolved inside that image.
# 2. The module must load and run prebuilt-abi-smoke.php on the newest image.
# 3. On the oldest image the module must refuse to start with its version
#    warning rather than load and die on an undefined symbol. When both images
#    are the same patch (the first release of a minor), this step is skipped.
set -Eeuo pipefail
shopt -s inherit_errexit

if [[ $# -ne 3 ]]; then
	echo "usage: $0 MODULE.so OLDEST_IMAGE NEWEST_IMAGE" >&2
	exit 2
fi

module=$(realpath "$1")
smoke_php=$(realpath "$(dirname "$0")/prebuilt-abi-smoke.php")

image_version() {
	local ref=${1%%@*}
	ref=${ref##*:}
	printf '%s\n' "${ref%%-*}"
}

# Pulls a digest-pinned official php image, trying the GHCR copy first and
# then the public mirrors of the Docker official images, all at the same
# digest: ECR Public and Docker Hub both enforce data limits on shared runner
# IPs. Prints the reference that was pulled.
pull_image() {
	local ref=$1 path prefix candidate attempt
	if [[ "$ref" == fastjson-local/* ]]; then
		if ! docker image inspect "$ref" >/dev/null 2>&1; then
			echo "::error::local image $ref was not built in this job" >&2
			return 1
		fi
		printf '%s\n' "$ref"
		return 0
	fi
	if [[ "$ref" != *@sha256:* ]]; then
		echo "::error::$ref is not pinned by digest" >&2
		return 1
	fi
	path=$ref
	for prefix in public.ecr.aws/docker/library/ mirror.gcr.io/library/ docker.io/library/; do
		path=${path#"$prefix"}
	done
	local -a candidates=()
	if [[ "$path" == php:* ]]; then
		candidates=("ghcr.io/iliaal/$path" "public.ecr.aws/docker/library/$path"
			"mirror.gcr.io/library/$path" "docker.io/library/$path")
	else
		candidates=("$ref")
	fi
	for candidate in "${candidates[@]}"; do
		for attempt in 1 2 3; do
			if docker pull -q "$candidate" >&2; then
				echo "pulled $candidate" >&2
				printf '%s\n' "$candidate"
				return 0
			fi
			if [[ $attempt -lt 3 ]]; then
				sleep $((attempt * 5))
			fi
		done
		echo "pull of $candidate failed; trying the next mirror" >&2
	done
	echo "::error::could not pull $ref from any mirror" >&2
	return 1
}

oldest=$(pull_image "$2")
newest=$(pull_image "$3")

# The module is NTS; a ZTS image would reject it at load for a reason the
# import check cannot see, so refuse it up front.
for image in "$oldest" "$newest"; do
	if ! docker run --rm "$image" php -n -r 'exit(PHP_ZTS ? 1 : 0);'; then
		echo "::error::$image is not a non-thread-safe PHP" >&2
		exit 1
	fi
done

work=$(mktemp -d)
cid=""
cleanup() {
	if [[ -n "$cid" ]]; then
		docker rm -f "$cid" >/dev/null 2>&1 || true
	fi
	rm -rf "$work"
}
trap cleanup EXIT

echo "== $(basename "$module"): import check against $newest"
cid=$(docker create "$newest")
mkdir -p "$work/providers"
docker cp -L "$cid:/usr/local/bin/php" "$work/providers/php"

ldd_out=$(docker run --rm -v "$module:/ext/module.so:ro" "$newest" ldd /ext/module.so)
printf '%s\n' "$ldd_out"
mapfile -t libs < <(awk '$2 == "=>" && $3 ~ /^\// { print $3 } $1 ~ /^\// { print $1 }' <<< "$ldd_out")
n=0
for lib in "${libs[@]}"; do
	n=$((n + 1))
	docker cp -L "$cid:$lib" "$work/providers/lib$n"
done

for provider in "$work"/providers/*; do
	nm -D --defined-only "$provider" | awk 'NF >= 3 { sub(/@.*/, "", $3); print $3 }'
done | sort -u > "$work/exported"

nm -D --undefined-only "$module" | awk '$1 == "U" { sub(/@.*/, "", $2); print $2 }' | sort -u > "$work/imported"

if [[ ! -s "$work/exported" ]]; then
	echo "::error::no exported symbols read from $newest; refusing to pass the import check"
	exit 1
fi

missing=$(comm -23 "$work/imported" "$work/exported")
imported_count=$(wc -l < "$work/imported")
if [[ -n "$missing" ]]; then
	echo "::error::$(basename "$module") imports symbols that $newest does not export:"
	printf '%s\n' "$missing"
	exit 1
fi
echo "import check OK: $imported_count imports, all exported by $newest"

status=0
want=$(image_version "$newest")
echo "== $(basename "$module"): load and smoke on $newest (PHP $want)"
set +e
out=$(docker run --rm \
	-v "$module:/ext/fastjson.so:ro" \
	-v "$smoke_php:/smoke.php:ro" \
	"$newest" php -n -d extension=/ext/fastjson.so /smoke.php 2>&1)
rc=$?
set -e
printf '%s\n' "$out"
if [[ $rc -ne 0 ]]; then
	echo "::error::smoke on $newest exited with status $rc"
	status=1
elif grep -qi 'undefined symbol' <<< "$out"; then
	echo "::error::smoke on $newest reported an undefined symbol"
	status=1
elif ! grep -qF "ok php=$want " <<< "$out"; then
	echo "::error::smoke on $newest did not report PHP $want"
	status=1
fi

old=$(image_version "$oldest")
if [[ "$old" == "$want" ]]; then
	echo "$newest is the first patch of its minor; no older patch to refuse"
	exit "$status"
fi
echo "== $(basename "$module"): refuses to start on $oldest (PHP $old)"
set +e
out=$(docker run --rm \
	-v "$module:/ext/fastjson.so:ro" \
	-v "$smoke_php:/smoke.php:ro" \
	"$oldest" php -n -d extension=/ext/fastjson.so /smoke.php 2>&1)
rc=$?
set -e
printf '%s\n' "$out"
if [[ $rc -eq 0 ]] || grep -qi 'undefined symbol' <<< "$out" \
		|| ! grep -qF "fastjson was built for PHP $want and cannot run on PHP $old" <<< "$out" \
		|| ! grep -qF 'Unable to start fastjson module' <<< "$out"; then
	echo "::error::$(basename "$module") did not refuse to start on $oldest (exit $rc)"
	status=1
fi
exit "$status"
