#!/usr/bin/env bash
# Exercise downloads offline in a disposable project, never the real corpus.
set -Eeuo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
readonly SCRIPT_DIR
work_dir=$(mktemp -d)
trap 'rm -rf -- "$work_dir"' EXIT
project="$work_dir/project with spaces"
mkdir -p "$project/bench" "$work_dir/bin"
cp "$SCRIPT_DIR/../bench/fetch-data.sh" "$project/bench/"

# Emulate a transfer that writes some bytes before failing, or returns an
# empty response successfully. All other URLs return a complete fixture.
cat > "$work_dir/bin/curl" <<'CURL'
#!/usr/bin/env bash
set -euo pipefail
[[ $# == 4 && $1 == -fsSL && $3 == -o ]]
name=${2#*/jsonexamples/}
printf '%s\n' "$name" >> "$FETCH_LOG"
if [[ "$name" == "$FAIL_FILE" ]]; then
    if [[ "$FAIL_MODE" == partial ]]; then
        printf '{"incomplete":' > "$4"
        exit 18
    fi
    : > "$4"
else
    printf '{"complete":true}\n' > "$4"
fi
CURL
chmod +x "$work_dir/bin/curl"
export PATH="$work_dir/bin:$PATH"
export FETCH_LOG="$work_dir/requests"
export FAIL_FILE FAIL_MODE

for FAIL_FILE in apache_builds.json small/adversarial.json; do
    for FAIL_MODE in partial empty; do
        rm -rf -- "$project/bench/data"
        : > "$FETCH_LOG"
        if bash "$project/bench/fetch-data.sh" > "$work_dir/output" 2>&1; then
            printf '%s transfer unexpectedly succeeded: %s\n' "$FAIL_MODE" "$FAIL_FILE" >&2
            exit 1
        fi
        [[ ! -e "$project/bench/data/$FAIL_FILE" ]] || {
            printf 'failed transfer left a cache entry: %s\n' "$FAIL_FILE" >&2
            exit 1
        }
        [[ -z "$(find "$project/bench/data" -name '*.tmp.*' -print)" ]]

        # Retry completes all 21 files, including the previously failed URL.
        failed_file=$FAIL_FILE
        FAIL_FILE=''
        bash "$project/bench/fetch-data.sh" > "$work_dir/output" 2>&1
        [[ $(find "$project/bench/data" -name '*.json' | wc -l) -eq 21 ]]
        [[ $(grep -Fxc "$failed_file" "$FETCH_LOG") -eq 2 ]]
        printf '{"complete":true}\n' > "$work_dir/expected"
        cmp "$work_dir/expected" "$project/bench/data/$failed_file"
        [[ -z "$(find "$project/bench/data" -name '*.tmp.*' -print)" ]]

        # Successful cached files must be reused without network requests.
        cp "$FETCH_LOG" "$work_dir/requests-before"
        bash "$project/bench/fetch-data.sh" > "$work_dir/output" 2>&1
        cmp "$work_dir/requests-before" "$FETCH_LOG"
        FAIL_FILE=$failed_file
    done
done
printf 'benchmark download regressions passed\n'
