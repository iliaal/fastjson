--TEST--
fastjson_encode: JSON_HEX_* on large strings (>= 256 KiB) and \uXXXX-adjacent escapes match ext/json
--EXTENSIONS--
fastjson
--FILE--
<?php

// fastjson_apply_hex_escapes() rewrites the emitted string region
// backwards, skipping whole escape sequences ('\"' pairs and \uXXXX
// runs). The small-string path is covered by encode_hex_flags.phpt;
// this test exercises the same invariant on the large-string paths in
// fastjson_write_large_json_string() (fused copy, mid-band tail fusion,
// exact-size preflight), where hex substitution runs over output
// produced by three different writer strategies.

$prefix = str_repeat('a', 256 * 1024);

// Mostly-clean ASCII with an escape-heavy tail crosses the mid-band
// tail-fusion path (prefix_len >= len - len/8).
$fused = $prefix . str_repeat('<>&\'\\', 40) . '"';

// Escape-dense from the first byte forces the exact-size preflight.
$dense = str_repeat('<>&\'"\\', 40000);

// Non-ASCII adjacent to quote/backslash: default escaping emits
// \uXXXX runs that the backward scan must skip whole.
$uni = str_repeat("é\\\"", 30000);

$cases = [
    [$prefix . '"', 0],
    [$prefix . '\\', 0],
    [$prefix . '<>&\'', 0],
    [$fused, 0],
    [$dense, 0],
    [$uni, 0],
    [$uni, JSON_UNESCAPED_UNICODE],
    [$prefix . "é\"\\", JSON_HEX_QUOT | JSON_HEX_AMP],
];

$flags = [
    "TAG"  => JSON_HEX_TAG,
    "APOS" => JSON_HEX_APOS,
    "QUOT" => JSON_HEX_QUOT,
    "AMP"  => JSON_HEX_AMP,
    "ALL"  => JSON_HEX_TAG | JSON_HEX_APOS | JSON_HEX_QUOT | JSON_HEX_AMP,
    "ALL+UNESC_SLASH" => JSON_HEX_TAG | JSON_HEX_APOS | JSON_HEX_QUOT | JSON_HEX_AMP | JSON_UNESCAPED_SLASHES,
];

$diverge = 0;
$broken = 0;
foreach ($cases as [$in, $extra]) {
    foreach ($flags as $name => $f) {
        $fl = $f | $extra;
        $fj = fastjson_encode($in, $fl);
        $rj = json_encode($in, $fl);
        if ($fj !== $rj) {
            $diverge++;
            printf("DIVERGE len=%d extra=%d flag=%s fj=%s rj=%s\n",
                strlen($in), $extra, $name,
                substr($fj, 0, 40), substr($rj, 0, 40));
        }
        if ($fj !== false && json_decode($fj) !== $in) {
            $broken++;
            printf("BROKEN  len=%d extra=%d flag=%s\n", strlen($in), $extra, $name);
        }
    }
}
echo "diverge=$diverge broken=$broken\n";
?>
--EXPECT--
diverge=0 broken=0
