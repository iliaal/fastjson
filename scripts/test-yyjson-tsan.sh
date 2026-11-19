#!/usr/bin/env bash
# Build the bundled yyjson with ThreadSanitizer and run scripts/yyjson-tsan.c,
# which decodes and encodes from several threads at once, as ZTS PHP
# (FrankenPHP, parallel) does. Any reported race fails the run.
set -Eeuo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
readonly SCRIPT_DIR
readonly PROJECT_DIR="${SCRIPT_DIR%/scripts}"
readonly RUNS="${TSAN_RUNS:-5}"
CC="${CC:-cc}"

work_dir=$(mktemp -d)
trap 'rm -rf -- "$work_dir"' EXIT

# The yyjson feature defines config.m4 compiles the extension with.
"$CC" -std=gnu11 -O1 -g -fsanitize=thread -pthread \
    -DYYJSON_DISABLE_INCR_READER=1 -DYYJSON_DISABLE_FILE=1 \
    -DYYJSON_DISABLE_UTILS=1 \
    -I"$PROJECT_DIR/vendor/yyjson" \
    "$SCRIPT_DIR/yyjson-tsan.c" "$PROJECT_DIR/vendor/yyjson/yyjson.c" \
    -o "$work_dir/yyjson-tsan"

# Kernels with high mmap randomization map memory where TSan does not
# expect it ("unexpected memory mapping"); run without ASLR.
run_prefix=()
if command -v setarch > /dev/null && setarch "$(uname -m)" -R true 2> /dev/null; then
    run_prefix=(setarch "$(uname -m)" -R)
fi

failures=0
for ((i = 1; i <= RUNS; i++)); do
    status=0
    TSAN_OPTIONS="halt_on_error=1:exitcode=66${TSAN_OPTIONS:+:$TSAN_OPTIONS}" \
        "${run_prefix[@]}" "$work_dir/yyjson-tsan" > "$work_dir/out" 2>&1 || status=$?
    if [[ "$status" -ne 0 ]] || ! grep -qx 'done' "$work_dir/out"; then
        printf 'run %d/%d failed (status %d):\n' "$i" "$RUNS" "$status" >&2
        cat "$work_dir/out" >&2
        failures=$((failures + 1))
    fi
done

if [[ "$failures" -ne 0 ]]; then
    printf 'yyjson TSan: %d of %d runs failed\n' "$failures" "$RUNS" >&2
    exit 1
fi
printf 'yyjson TSan: %d of %d runs clean\n' "$RUNS" "$RUNS"
