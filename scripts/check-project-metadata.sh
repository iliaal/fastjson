#!/usr/bin/env bash
set -Eeuo pipefail

readonly upstream_dir="tests/upstream-json"

generated=$(find "${upstream_dir}" -maxdepth 1 -type f -name '*.phpt' | wc -l)
generated=${generated//[[:space:]]/}
skipped=$(awk '!/^[[:space:]]*(#|$)/ { count++ } END { print count + 0 }' \
    "${upstream_dir}/.skiplist")
total=$((generated + skipped))
patches=$(grep -Ec '^## P-[0-9]{3}:' vendor/yyjson/PATCHES.md)

grep -Fq "| Synced count       | ${total} |" "${upstream_dir}/README.md"
grep -Fq "| Skipped            | ${skipped} (per .skiplist) |" \
    "${upstream_dir}/README.md"
grep -Fq "| Total upstream tests | ${total} |" "${upstream_dir}/STATE.md"
grep -Fq "| Skipped (categorized) | ${skipped} |" "${upstream_dir}/STATE.md"
grep -Fq "| Rewritten + run | ${generated} |" "${upstream_dir}/STATE.md"
grep -Fq -- "- ${generated}-test compat harness" README.md

if [[ "${patches}" -ne 4 ]]; then
    echo "Expected four documented yyjson patches, found ${patches}" >&2
    exit 1
fi
grep -Fq 'with four local patches (P-001 through P-004)' README.md

printf 'metadata counts verified: %d upstream, %d generated, %d skipped, %d patches\n' \
    "${total}" "${generated}" "${skipped}" "${patches}"
