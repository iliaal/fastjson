#!/usr/bin/env bash
# Exercise the generator in a disposable project, never the checked-in suite.
set -Eeuo pipefail

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)
readonly SCRIPT_DIR
work_dir=$(mktemp -d)
trap 'rm -rf -- "$work_dir"' EXIT

project="$work_dir/project with spaces"
source_dir="$work_dir/php source"
mkdir -p "$project/scripts" "$project/tests/upstream-json" \
    "$source_dir/ext/json/tests" "$source_dir/main"
cp "$SCRIPT_DIR/sync-upstream-json-tests.sh" "$project/scripts/"
dest="$project/tests/upstream-json"
for name in old.phpt .manifest .source-revision README.md; do
    printf 'original %s\n' "$name" > "$dest/$name"
done
printf 'skip.phpt # fixture skip\n' > "$dest/.skiplist"
cp -R "$dest" "$work_dir/before"
cat > "$source_dir/main/php_version.h" <<'HEADER'
#define PHP_MAJOR_VERSION 8
#define PHP_MINOR_VERSION 4
#define PHP_RELEASE_VERSION 0
HEADER

git -C "$source_dir" init -q
git -C "$source_dir" -c user.name=Test -c user.email=test@example.invalid \
    -c commit.gpgsign=false commit -q --allow-empty -m fixture

# A valid source checkout with no tests must fail before modifying any output.
if bash "$project/scripts/sync-upstream-json-tests.sh" "$source_dir" \
    > "$work_dir/output" 2>&1; then
    echo 'empty source unexpectedly succeeded' >&2
    exit 1
fi
grep -Fq 'contains no .phpt tests; existing suite left unchanged' "$work_dir/output"
diff -r "$work_dir/before" "$dest"

# Populated sources still regenerate, rewrite symbols, honor skips and replace
# stale output. Quoted array expansion must also work with spaces in the path.
cat > "$source_dir/ext/json/tests/basic.phpt" <<'PHPT'
--TEST--
JSON fixture
--FILE--
<?php var_dump(json_encode(null)); ?>
--EXPECT--
string(4) "null"
PHPT
cp "$source_dir/ext/json/tests/basic.phpt" "$source_dir/ext/json/tests/skip.phpt"
bash "$project/scripts/sync-upstream-json-tests.sh" "$source_dir" > "$work_dir/output"
[[ ! -e "$dest/old.phpt" && ! -e "$dest/skip.phpt" ]]
grep -Fq 'fastjson_encode(null)' "$dest/basic.phpt"
grep -Fxq fastjson "$dest/basic.phpt"
printf 'basic.phpt\nskip.phpt\n' > "$work_dir/expected-manifest"
cmp "$work_dir/expected-manifest" "$dest/.manifest"
git -C "$source_dir" rev-parse HEAD > "$work_dir/expected-revision"
cmp "$work_dir/expected-revision" "$dest/.source-revision"
cmp "$work_dir/before/.skiplist" "$dest/.skiplist"
grep -Fq '| Synced count       | 2 |' "$dest/README.md"
grep -Fq '| Skipped            | 1 (per .skiplist) |' "$dest/README.md"

# Existing extension requirements must survive the rewrite, with fastjson
# added exactly once. Also pin the multiline --TEST-- insertion behavior.
for fixture in existing already multiline trailing crlf; do
    cat > "$source_dir/ext/json/tests/$fixture.phpt" <<'PHPT'
--TEST--
JSON fixture
A second description line
PHPT
    case "$fixture" in
        existing|crlf) printf '%s\n' --EXTENSIONS-- json mbstring ;;
        already) printf '%s\n' --EXTENSIONS-- json '  fastjson  ' ;;
    esac >> "$source_dir/ext/json/tests/$fixture.phpt"
    cat >> "$source_dir/ext/json/tests/$fixture.phpt" <<'PHPT'
--FILE--
<?php var_dump(json_encode(null)); ?>
--EXPECT--
string(4) "null"
PHPT
    if [[ "$fixture" == trailing ]]; then
        printf '%s\n' --EXTENSIONS-- json >> "$source_dir/ext/json/tests/$fixture.phpt"
    fi
    if [[ "$fixture" == crlf ]]; then
        sed 's/$/\r/' "$source_dir/ext/json/tests/$fixture.phpt" > "$work_dir/crlf"
        mv "$work_dir/crlf" "$source_dir/ext/json/tests/$fixture.phpt"
    fi
done
bash "$project/scripts/sync-upstream-json-tests.sh" "$source_dir" > "$work_dir/output"
for fixture in existing already multiline trailing crlf; do
    awk '
        /^--EXTENSIONS--/ { sections++; in_extensions = 1; next }
        /^--[A-Z_]+--/ { in_extensions = 0 }
        in_extensions && /^[[:space:]]*fastjson[[:space:]]*$/ { fastjson++ }
        END { exit !(sections == 1 && fastjson == 1) }
    ' "$dest/$fixture.phpt" || {
        printf 'missing or duplicate fastjson requirement: %s\n' "$fixture" >&2
        exit 1
    }
    grep -Fq 'fastjson_encode(null)' "$dest/$fixture.phpt"
    head -n 3 "$dest/$fixture.phpt" > "$work_dir/description"
    head -n 3 "$source_dir/ext/json/tests/$fixture.phpt" > "$work_dir/expected-description"
    cmp "$work_dir/expected-description" "$work_dir/description"
done
# Apart from adding fastjson, the existing extension block stays intact.
printf '%s\n' json mbstring fastjson > "$work_dir/expected-extensions"
awk '/^--EXTENSIONS--/ { active = 1; next }
     /^--[A-Z_]+--/ { active = 0 }
     active { print }' "$dest/existing.phpt" > "$work_dir/extensions"
cmp "$work_dir/expected-extensions" "$work_dir/extensions"
sed 's/json_encode/fastjson_encode/g' "$source_dir/ext/json/tests/already.phpt" \
    > "$work_dir/expected-already"
cmp "$work_dir/expected-already" "$dest/already.phpt"
# The hand-maintained skiplist accepts whitespace and indented comments,
# matching the metadata checker. Every spelling must preserve the same skips.
for entry in '  skip.phpt # indented entry' $'\tskip.phpt\t# tab-separated reason' 'skip.phpt'; do
    printf '%s\n' '' '   ' $'\t' '  # indented comment' $'\t# tabbed comment' \
        > "$dest/.skiplist"
    # The final entry deliberately has no newline, as in a hand-edited file.
    printf '%s' "$entry" >> "$dest/.skiplist"
    cp "$dest/.skiplist" "$work_dir/expected-skiplist"
    bash "$project/scripts/sync-upstream-json-tests.sh" "$source_dir" > "$work_dir/output"
    [[ ! -e "$dest/skip.phpt" && -f "$dest/basic.phpt" ]]
    cmp "$work_dir/expected-skiplist" "$dest/.skiplist"
    grep -Fq '| Skipped            | 1 (per .skiplist) |' "$dest/README.md"
done
printf 'upstream sync regressions passed\n'
