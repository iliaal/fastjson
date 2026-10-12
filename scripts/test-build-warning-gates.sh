#!/usr/bin/env bash
# Exercise the real workflow gates offline, including logs larger than a pipe.
set -Eeuo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
readonly SCRIPT_DIR
work_dir=$(mktemp -d)
trap 'rm -rf -- "$work_dir"' EXIT

# Extract every compiler-warning gate, keeping its diagnostic and exit status.
# Run the snippets with pipefail, just like the workflow build steps.
awk -v dest="$work_dir" '
    /if grep '\''warning:'\'' build.log/ { active = 1; count++ }
    active { print > (dest "/gate-" count ".sh") }
    active && /^[[:space:]]*fi[[:space:]]*$/ { active = 0 }
    END { if (active || count != 4) exit 1 }
' "$SCRIPT_DIR/../.github/workflows/tests.yml" \
  "$SCRIPT_DIR/../.github/workflows/release-linux.yml"

cd "$work_dir"
for fixture in empty clean linker single adjacent blank large; do
    : > build.log
    case "$fixture" in
        clean) printf 'Compiling fastjson.c\nBuild complete.\n' > build.log ;;
        linker) printf 'ld: warning: deprecated option\n' > build.log ;;
        single) printf 'fastjson.c:42: warning: fixture\n' > build.log ;;
        adjacent)
            printf 'ld: warning: deprecated option\nfastjson.c:42: warning: fixture\n' > build.log ;;
        blank)
            printf 'ld: warning: deprecated option\n\nfastjson.c:42: warning: fixture\n' > build.log ;;
        large)
            awk 'BEGIN {
                for (i = 0; i < 10000; i++) print "fastjson.c:42: warning: fixture"
            }' > build.log ;;
    esac
    for gate in gate-*.sh; do
        status=0
        bash -e -o pipefail "$gate" > output 2>&1 || status=$?
        case "$fixture" in
            empty|clean|linker)
                [[ "$status" -eq 0 && ! -s output ]] || {
                    printf '%s rejected %s log\n' "$gate" "$fixture" >&2
                    exit 1
                } ;;
            *)
                [[ "$status" -eq 1 ]] &&
                    grep -Fq '::error::Build produced warning(s)' output &&
                    grep -Fq 'fastjson.c:42: warning: fixture' output || {
                        printf '%s accepted or mishandled %s warning log (status %s)\n' \
                            "$gate" "$fixture" "$status" >&2
                        exit 1
                    } ;;
        esac
    done
done
printf 'all four build warning gates passed regressions\n'
