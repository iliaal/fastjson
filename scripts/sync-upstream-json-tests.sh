#!/usr/bin/env bash
#
# Mirror php-src/ext/json/tests/*.phpt into tests/upstream-json/ with
# mechanical symbol rewrites so the upstream suite runs against
# fastjson_*. Re-run after a PHP upgrade or when ext/json gains new
# tests; the destination directory is regenerated wholesale.
#
# Usage: scripts/sync-upstream-json-tests.sh [path/to/php-src]
#   default php-src path: ~/php-src
#
# Source-of-truth annotations:
# - Each generated file carries an `# Generated from <upstream>` header
#   so reviewers can trace divergences back to the PHP version.
# - tests/upstream-json/.skiplist documents tests intentionally skipped
#   (with one-line reasons). The script does NOT auto-modify the
#   skiplist; it's authored by hand based on harness output.
# - .source-revision and .manifest pin the exact php-src snapshot and its
#   complete test-name set so additions/removals cannot cancel out in a
#   count-only metadata check.

set -euo pipefail

PHP_SRC="${1:-$HOME/php-src}"
SRC_DIR="$PHP_SRC/ext/json/tests"
DEST_DIR="$(cd "$(dirname "$0")/.." && pwd)/tests/upstream-json"

if [ ! -d "$SRC_DIR" ]; then
    echo "error: $SRC_DIR does not exist" >&2
    exit 1
fi

# Resolve the PHP version we're syncing from for the header line.
PHP_VERSION=$(grep -oP 'PHP_(MAJOR|MINOR|RELEASE)_VERSION\s+\K\d+' \
                  "$PHP_SRC/main/php_version.h" 2>/dev/null \
              | paste -sd.)
PHP_VERSION="${PHP_VERSION:-unknown}"
PHP_SOURCE_COMMIT=$(git -C "$PHP_SRC" rev-parse HEAD 2>/dev/null || true)
PHP_SOURCE_COMMIT="${PHP_SOURCE_COMMIT:-unknown}"

mkdir -p "$DEST_DIR"
# Wipe existing entries -- they're all regenerable, no manual edits.
find "$DEST_DIR" -maxdepth 1 -name '*.phpt' -delete

printf '%s\n' "$SRC_DIR"/*.phpt | while IFS= read -r src; do
    basename "$src"
done | sort > "$DEST_DIR/.manifest"
printf '%s\n' "$PHP_SOURCE_COMMIT" > "$DEST_DIR/.source-revision"

count_total=0
count_skipped_by_skiplist=0

# Read skiplist (lines: <basename>.phpt # reason).
declare -A SKIPLIST=()
if [ -f "$DEST_DIR/.skiplist" ]; then
    while IFS= read -r line; do
        [ -z "$line" ] && continue
        case "$line" in \#*) continue ;; esac
        name="${line%%#*}"
        name="${name## }"
        name="${name%% *}"
        SKIPLIST["$name"]=1
    done < "$DEST_DIR/.skiplist"
fi

for src in "$SRC_DIR"/*.phpt; do
    base="$(basename "$src")"
    count_total=$((count_total + 1))

    if [ -n "${SKIPLIST[$base]:-}" ]; then
        count_skipped_by_skiplist=$((count_skipped_by_skiplist + 1))
        continue
    fi

    dest="$DEST_DIR/$base"

    # Pipeline:
    # 1. Replace function names. Order matters: longer matches first
    #    (json_last_error_msg before json_last_error) so we don't half-
    #    rewrite the longer name.
    # 2. Inject --EXTENSIONS-- fastjson after --TEST-- if missing.
    # 3. Prepend a generator comment in the --DESCRIPTION-- area (we
    #    use a leading PHPT comment that PHPT ignores).
    sed -E \
        -e 's/\bjson_last_error_msg\b/fastjson_last_error_msg/g' \
        -e 's/\bjson_last_error\b/fastjson_last_error/g' \
        -e 's/\bjson_validate\b/fastjson_validate/g' \
        -e 's/\bjson_encode\b/fastjson_encode/g' \
        -e 's/\bjson_decode\b/fastjson_decode/g' \
        "$src" > "$dest.tmp"

    # Add --EXTENSIONS-- fastjson if not already present. The --TEST--
    # block can span multiple lines (test name + extra description),
    # so consume everything until the next --SECTION-- before injecting,
    # otherwise --EXTENSIONS-- splits the description and PHPT treats
    # the trailing prose as a missing-extension name (silent SKIP).
    if ! grep -q '^--EXTENSIONS--' "$dest.tmp"; then
        awk -v hdr="--EXTENSIONS--\nfastjson" '
            /^--TEST--/ {
                print
                while ((getline line) > 0) {
                    if (line ~ /^--[A-Z_]+--/) {
                        print hdr
                        print line
                        next
                    }
                    print line
                }
                next
            }
            { print }
        ' "$dest.tmp" > "$dest.tmp2"
        mv "$dest.tmp2" "$dest.tmp"
    fi

    # PHPT requires --TEST-- as the very first line, so source-
    # attribution lives in tests/upstream-json/README.md, not inline.
    mv "$dest.tmp" "$dest"
done

# Refresh the README so reviewers can trace which PHP version this
# directory was synced from.
cat > "$DEST_DIR/README.md" <<EOF
# upstream-json compat harness

These tests are mechanically rewritten copies of
\`php-src/ext/json/tests/*.phpt\` with \`json_*\` symbol references
remapped to \`fastjson_*\`. The harness measures how closely fastjson
tracks ext/json's contract on real-world inputs.

**Do not edit files in this directory by hand.** Regenerate with:

\`\`\`sh
scripts/sync-upstream-json-tests.sh [\$HOME/php-src]
\`\`\`

| Source PHP version | $PHP_VERSION |
| Source php-src commit | $PHP_SOURCE_COMMIT |
| Synced count       | $count_total |
| Skipped            | $count_skipped_by_skiplist (per .skiplist) |

Tests intentionally not rewritten are listed in \`.skiplist\` with a
one-line reason; categories are documented at the head of that file.
EOF

count_synced=$(find "$DEST_DIR" -maxdepth 1 -name '*.phpt' | wc -l)

cat <<EOF
upstream-json sync complete:
  source:       $SRC_DIR
  destination:  $DEST_DIR
  PHP version:  $PHP_VERSION
  source commit: $PHP_SOURCE_COMMIT
  total tests:  $count_total
  synced:       $count_synced
  skipped:      $count_skipped_by_skiplist (per .skiplist)

Run the rewritten suite:
  TEST_PHP_EXECUTABLE=\$(which php) \\
  TEST_PHP_ARGS="-d extension=\$(pwd)/modules/fastjson.so" \\
    php run-tests.php tests/upstream-json/
EOF
