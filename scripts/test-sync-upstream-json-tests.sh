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
printf 'upstream sync regressions passed\n'
