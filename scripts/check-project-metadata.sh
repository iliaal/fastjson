#!/usr/bin/env bash
set -Eeuo pipefail

readonly upstream_dir="tests/upstream-json"

source_revision=$(<"${upstream_dir}/.source-revision")
if [[ ! "${source_revision}" =~ ^[0-9a-f]{40}$ ]]; then
    echo "Invalid pinned php-src revision: ${source_revision}" >&2
    exit 1
fi
grep -Fq "| Source php-src commit | ${source_revision} |" \
    "${upstream_dir}/README.md"

generated=$(find "${upstream_dir}" -maxdepth 1 -type f -name '*.phpt' | wc -l)
generated=${generated//[[:space:]]/}
skipped=$(awk '!/^[[:space:]]*(#|$)/ { count++ } END { print count + 0 }' \
    "${upstream_dir}/.skiplist")
total=$((generated + skipped))
manifest_count=$(wc -l <"${upstream_dir}/.manifest")
manifest_count=${manifest_count//[[:space:]]/}

if [[ "${total}" -ne "${manifest_count}" ]]; then
    echo "Manifest has ${manifest_count} tests, generated + skipped has ${total}" >&2
    exit 1
fi

while IFS= read -r name; do
    generated_match=0
    skipped_match=0
    [[ -f "${upstream_dir}/${name}" ]] && generated_match=1
    awk -v name="${name}" \
        '!/^[[:space:]]*(#|$)/ && $1 == name { found = 1 } \
         END { exit !found }' "${upstream_dir}/.skiplist" &&
        skipped_match=1
    if [[ $((generated_match + skipped_match)) -ne 1 ]]; then
        echo "Manifest entry must be exactly generated or skipped: ${name}" >&2
        exit 1
    fi
done <"${upstream_dir}/.manifest"

# The loop body must stay in this shell; failures inside these read-only
# producer pipelines cannot be surfaced separately by process substitution.
# shellcheck disable=SC2312
while IFS= read -r name; do
    grep -Fxq "${name}" "${upstream_dir}/.manifest" || {
        echo "Generated test is absent from manifest: ${name}" >&2
        exit 1
    }
done < <(find "${upstream_dir}" -maxdepth 1 -type f -name '*.phpt' \
    -printf '%f\n' | sort)

# shellcheck disable=SC2312
while IFS= read -r name; do
    grep -Fxq "${name}" "${upstream_dir}/.manifest" || {
        echo "Skipped test is absent from manifest: ${name}" >&2
        exit 1
    }
done < <(awk '!/^[[:space:]]*(#|$)/ { print $1 }' \
    "${upstream_dir}/.skiplist" | sort)
patches=$(grep -Ec '^## P-[0-9]{3}:' vendor/yyjson/PATCHES.md)

stub_hash=$(sha1sum fastjson.stub.php)
stub_hash=${stub_hash%% *}
embedded_stub_hashes=()
while IFS= read -r line; do
    if [[ "${line}" =~ ^[[:space:]]\*[[:space:]]Stub[[:space:]]hash:[[:space:]]([0-9a-f]{40})[[:space:]]\*/$ ]]; then
        embedded_stub_hashes+=("${BASH_REMATCH[1]}")
    fi
done <fastjson_arginfo.h
if [[ "${#embedded_stub_hashes[@]}" -ne 1 ||
    "${embedded_stub_hashes[0]}" != "${stub_hash}" ]]; then
    printf 'fastjson_arginfo.h is stale; regenerate it from fastjson.stub.php\n' >&2
    exit 1
fi

grep -Fq "| Synced count       | ${total} |" "${upstream_dir}/README.md"
grep -Fq "| Skipped            | ${skipped} (per .skiplist) |" \
    "${upstream_dir}/README.md"
grep -Fq "| Total upstream tests | ${total} |" "${upstream_dir}/STATE.md"
grep -Fq "| Skipped (categorized) | ${skipped} |" "${upstream_dir}/STATE.md"
grep -Fq "| Rewritten + run | ${generated} |" "${upstream_dir}/STATE.md"
grep -Fq -- "- ${generated}-test compat harness" README.md

if [[ "${patches}" -ne 5 ]]; then
    echo "Expected five documented yyjson patches, found ${patches}" >&2
    exit 1
fi
grep -Fq 'with five local patches (P-001 through P-005)' README.md

# shellcheck disable=SC2312
mapfile -t unix_sources < <(
    awk -F'"' '/WRAPPER_SOURCES=/{print $2}' config.m4 | tr ' ' '\n' |
        grep -E '\.c$' | sort
)
# shellcheck disable=SC2312
mapfile -t windows_sources < <(
    sed -n '/var wrapper_sources/,/;/p' config.w32 |
        grep -oE 'fastjson[_a-z]*\.c' | sort
)
if ! diff -u \
    <(printf '%s\n' "${unix_sources[@]}") \
    <(printf '%s\n' "${windows_sources[@]}"); then
    echo 'config.m4 and config.w32 wrapper source manifests differ' >&2
    exit 1
fi
grep -Fq "YYJSON_SOURCES=\"\$YYJSON_SRC_DIR/yyjson.c\"" config.m4
grep -Fq '"yyjson.c", "fastjson"' config.w32

printf 'metadata verified: %d upstream, %d generated, %d skipped, %d patches; source manifests match\n' \
    "${total}" "${generated}" "${skipped}" "${patches}"
