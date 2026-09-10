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

for name in "${FILES[@]}"; do
    target="$DEST/$name"
    if [ -s "$target" ]; then
        echo "  cached    $name ($(stat -c%s "$target") bytes)"
        continue
    fi
    echo "  fetching  $name"
    curl -fsSL "$BASE/$name" -o "$target"
    echo "            $(stat -c%s "$target") bytes"
done

for name in "${SMALL_FILES[@]}"; do
    target="$DEST/small/$name"
    if [ -s "$target" ]; then
        echo "  cached    small/$name ($(stat -c%s "$target") bytes)"
        continue
    fi
    echo "  fetching  small/$name"
    curl -fsSL "$BASE/small/$name" -o "$target"
    echo "            $(stat -c%s "$target") bytes"
done

echo
echo "Total: $(du -sh "$DEST" | cut -f1) in $DEST"
