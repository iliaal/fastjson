--TEST--
fastjson_encode: integer-valued doubles outside zend_long bounds round-trip cleanly (no 32-bit overflow)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Out-of-range doubles must bypass the zend_long cast, especially on 32-bit.
 * Compare numeric values: stripping .0 can make decoded doubles become ints. */

$cases = [
    0.0,
    1.0,
    -1.0,
    2147483647.0,           /* INT32_MAX exact */
    -2147483647.0,
    1.0e9,                  /* fits in INT32 */
    1.0e10,                 /* exceeds INT32, fits in INT64 */
    -1.0e10,
    1.0e12,
    -1.0e12,
    9.9e14,
    1.0e15,
    1.0e16,                 /* covered by widened 64-bit shortcut */
    -1.0e16,
    1.5e16,
    2.5e16,
    9.99e16,
    -9.99e16,
    1.0e17,                 /* shortcut excluded */
    1.0e18,
];

foreach ($cases as $v) {
    $enc = fastjson_encode($v);
    $back = fastjson_decode($enc);
    if ((float)$back != (float)$v) {
        echo "ROUNDTRIP FAIL on ", var_export($v, true),
             ": encoded=", $enc, " decoded=", var_export($back, true), "\n";
    }
}
echo "roundtrip OK\n";

/* Assert byte parity within the architecture's shortcut range; yyjson's
 * REAL formatting can diverge outside it. PHP switches to scientific at 1e17. */
$shortcut_cases = PHP_INT_SIZE >= 8
    ? [0.0, 1.0, -1.0, 2147483647.0, -2147483647.0, 1.0e9, 1.0e10, -1.0e10,
       1.0e12, -1.0e12, 9.9e14, 1.0e15, 1.0e16, -1.0e16, 1.5e16, 2.5e16,
       9.99e16, -9.99e16]
    : [0.0, 1.0, -1.0, 2147483647.0, -2147483647.0, 1.0e9];

foreach ($shortcut_cases as $v) {
    $fj = fastjson_encode($v);
    $std = json_encode($v);
    if ($fj !== $std) {
        echo "PARITY FAIL on ", var_export($v, true),
             ": fastjson=", $fj, " json=", $std, "\n";
    }
}
echo "shortcut parity OK\n";

$out = fastjson_encode(['a' => 1.0e10, 'b' => -1.0e10]);
$back = fastjson_decode($out, true);
var_dump((float)$back['a'] == 1.0e10 && (float)$back['b'] == -1.0e10);
?>
--EXPECT--
roundtrip OK
shortcut parity OK
bool(true)
