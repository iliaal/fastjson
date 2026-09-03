#!/usr/bin/env bash
set -Eeuo pipefail
shopt -s inherit_errexit

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
readonly SCRIPT_DIR
readonly PROJECT_DIR="${SCRIPT_DIR%/scripts}"
readonly PATCH_DIR="${PROJECT_DIR}/vendor/yyjson/patches"
readonly VERSION_FILE="${PROJECT_DIR}/vendor/yyjson/VERSION"
readonly ARCHIVE_SHA256="b16246f617b2a136c78d73e5e2647c6f1de1313e46678062985bdcf1f40bb75d"
readonly EXPECTED_PATCH_COUNT=5

_tmpdir=""

cleanup() {
	if [[ -n "${_tmpdir}" && -d "${_tmpdir}" ]]; then
		rm -rf -- "${_tmpdir}"
	fi
}

trap 'printf "Error at %s:%d\n" "${BASH_SOURCE[0]}" "$LINENO" >&2' ERR
trap cleanup EXIT

require() {
	command -v "$1" &>/dev/null || {
		printf 'Missing required command: %s\n' "$1" >&2
		exit 1
	}
}

pin_overflow_contract() {
	# CR-007 contract: fastjson's exponent-overflow (1e309 -> INF) retry
	# keys on err->code == YYJSON_READ_ERROR_INVALID_NUMBER, never on
	# yyjson's message wording. Pin both sides of that contract: the
	# vendored overflow diagnostic must still flow through an
	# INVALID_NUMBER site, and fastjson_decode.c must key on the code.
	local yyjson_c="${PROJECT_DIR}/vendor/yyjson/yyjson.c"
	local decode_c="${PROJECT_DIR}/fastjson_decode.c"
	if ! grep -q 'number is infinity when parsed as double' "${yyjson_c}"; then
		printf 'CR-007 contract broken: overflow diagnostic missing from %s\n' \
			"${yyjson_c}" >&2
		exit 1
	fi
	local bad
	bad=$(awk '/^fail_number:/{ buf=$0; getline nxt; if (buf !~ /INVALID_NUMBER/ && nxt !~ /INVALID_NUMBER/) print FNR": "buf" / "nxt; }' \
		"${yyjson_c}")
	if [[ -n "${bad}" ]]; then
		printf 'CR-007 contract broken: fail_number site without INVALID_NUMBER:\n%s\n' \
			"${bad}" >&2
		exit 1
	fi
	if grep -q 'number is infinity' "${decode_c}"; then
		printf 'CR-007 contract broken: fastjson_decode.c keys on yyjson message wording\n' >&2
		exit 1
	fi
	if ! grep -q 'err->code == YYJSON_READ_ERROR_INVALID_NUMBER' "${decode_c}"; then
		printf 'CR-007 contract broken: overflow retry lost its read-code key\n' >&2
		exit 1
	fi
}

main() {
	require awk
	require cmp
	require curl
	require git
	require sha256sum
	require tar

	local version
	version=$(<"${VERSION_FILE}")
	if [[ ! "${version}" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]]; then
		printf 'Invalid yyjson version: %s\n' "${version}" >&2
		exit 1
	fi

	_tmpdir=$(mktemp -d)
	local -r archive="${_tmpdir}/yyjson.tar.gz"
	local -r source_dir="${_tmpdir}/source"
	local -r work_dir="${_tmpdir}/work"
	local -r url="https://github.com/ibireme/yyjson/archive/refs/tags/${version}.tar.gz"

	curl --fail --location --silent --show-error \
		--connect-timeout 10 --max-time 60 \
		--retry 3 --retry-all-errors --retry-delay 1 --retry-max-time 180 \
		--output "${archive}" "${url}"
	printf '%s  %s\n' "${ARCHIVE_SHA256}" "${archive}" | sha256sum --check --status
	mkdir -p -- "${source_dir}" "${work_dir}/vendor/yyjson"
	tar --extract --gzip --file "${archive}" --strip-components=1 \
		--directory "${source_dir}"
	cp -- "${source_dir}/src/yyjson.c" "${work_dir}/vendor/yyjson/yyjson.c"
	cp -- "${source_dir}/src/yyjson.h" "${work_dir}/vendor/yyjson/yyjson.h"

	local patch_name
	local patch_path
	local patch_count=0
	while IFS= read -r patch_name; do
		[[ -n "${patch_name}" ]] || continue
		if [[ ! "${patch_name}" =~ ^[0-9]{4}-[a-z0-9-]+\.patch$ ]]; then
			printf 'Invalid patch-series entry: %s\n' "${patch_name}" >&2
			exit 1
		fi
		patch_path="${PATCH_DIR}/${patch_name}"
		if [[ ! -f "${patch_path}" ]]; then
			printf 'Missing yyjson patch: %s\n' "${patch_path}" >&2
			exit 1
		fi
		git -C "${work_dir}" apply --check -- "${patch_path}"
		git -C "${work_dir}" apply -- "${patch_path}"
		((patch_count += 1))
	done <"${PATCH_DIR}/series"
	if [[ "${patch_count}" -ne "${EXPECTED_PATCH_COUNT}" ]]; then
		printf 'Expected %d yyjson patches, found %d\n' \
			"${EXPECTED_PATCH_COUNT}" "${patch_count}" >&2
		exit 1
	fi

	cmp --silent -- "${work_dir}/vendor/yyjson/yyjson.c" \
		"${PROJECT_DIR}/vendor/yyjson/yyjson.c"
	cmp --silent -- "${work_dir}/vendor/yyjson/yyjson.h" \
		"${PROJECT_DIR}/vendor/yyjson/yyjson.h"
	pin_overflow_contract
	printf 'yyjson %s patch series verified: %d patches\n' \
		"${version}" "${patch_count}"
}

[[ "${BASH_SOURCE[0]}" == "$0" ]] && main "$@"
