#!/usr/bin/env bash
# Reuse simdjson_php's corpus (originally nativejson-benchmark/simdjson)
# for comparable results. bench/data/ is gitignored and regenerable.

set -euo pipefail

DEST="$(cd "$(dirname "$0")" && pwd)/data"
mkdir -p "$DEST"

BASE="https://raw.githubusercontent.com/crazyxman/simdjson_php/master/jsonexamples"

# Curated subset covering distinct shapes:
#   twitter        - deeply nested mixed (API response)
#   citm_catalog   - large catalog, many strings + integer keys
#   canada         - float-heavy GeoJSON (worst case for float formatting)
#   gsoc-2018      - deep + flat strings, larger doc
#   github_events  - small, common API response shape
#   random         - mixed types, moderate size
#   numbers        - number-heavy: integer + float boundary cases
#   stringifiedphp - PHP-derived data round-tripped through ext/json
FILES=(
    apache_builds.json
    canada.json
    citm_catalog.json
    github_events.json
    gsoc-2018.json
    instruments.json
    marine_ik.json
    mesh.json
    mesh.pretty.json
    numbers.json
    random.json
    stringifiedphp.json
    twitter.json
    twitterescaped.json
    update-center.json
)

# Small inputs expose function-dispatch and zval-setup overhead.
SMALL_FILES=(
    adversarial.json
    demo.json
    flatadversarial.json
    repeat.json
    truenull.json
    twitter_timeline.json
)

mkdir -p "$DEST/small"

# Publish a cache entry only after a complete, nonempty transfer. A failed
# curl can leave partial bytes behind; downloading directly to target would
# make the next run silently reuse those bytes via the -s cache check.
partial=""
trap 'if [[ -n "$partial" ]]; then rm -f -- "$partial"; fi' EXIT

fetch_file() {
    local name="$1"
    local target="$DEST/$name"
    if [ -s "$target" ]; then
        echo "  cached    $name ($(wc -c < "$target") bytes)"
        return
    fi
    echo "  fetching  $name"
    partial=$(mktemp "$target.tmp.XXXXXX")
    curl -fsSL "$BASE/$name" -o "$partial"
    if [ ! -s "$partial" ]; then
        echo "error: empty download for $name" >&2
        exit 1
    fi
    mv -- "$partial" "$target"
    partial=""
    echo "            $(wc -c < "$target") bytes"
}

for name in "${FILES[@]}"; do
    fetch_file "$name"
done
for name in "${SMALL_FILES[@]}"; do
    fetch_file "small/$name"
done

echo
echo "Total: $(du -sh "$DEST" | cut -f1) in $DEST"
