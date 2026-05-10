#!/usr/bin/env bash
#
# Fetch canonical JSON benchmark inputs into bench/data/. The files
# come from crazyxman/simdjson_php's jsonexamples directory -- the
# nearest cross-comparison surface (simdjson_php is the existing
# simdjson-backed PHP extension; using their corpus keeps the
# numbers comparable). Same files are originally from Milo Yip's
# nativejson-benchmark / simdjson upstream, so the suite is also
# apples-to-apples with yyjson / RapidJSON / nlohmann published
# numbers.
#
# The data directory is gitignored: ~16MB total when all files are
# fetched, regenerable on demand.

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

# Smaller files exercise the per-call-overhead regime where function
# dispatch + zval setup dominates throughput. simdjson_php groups them
# under jsonexamples/small.
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
