--TEST--
fastjson_encode: integer-valued doubles outside zend_long bounds round-trip cleanly (no 32-bit overflow)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Regression: the dw_emit_double integer-shortcut used a bound of 1e15,
 * which exceeds 32-bit zend_long range (ZEND_LONG_MAX ~ 2.147e9) and
 * invoked UB in the float-to-signed-int conversion on 32-bit PHP. With
 * the bug, fastjson_encode(1e10) on 32-bit emitted garbage like
 * "-2147483648" instead of a value that round-trips to 1e10. The fix
 * gates the shortcut on SIZEOF_ZEND_LONG: 1e15 on 64-bit;
 * ZEND_LONG_MAX on 32-bit. Out-of-range integer-valued doubles must
 * fall through to yyjson_write_number's REAL path.
 *
 * Numeric round-trip (loose `==`) is the right invariant: ext/json and
 * fastjson both strip the .0 by default, so an integer-valued double
 * decodes as int. The byte form differs between arches once the value
 * leaves the long shortcut, but the numeric value must always survive. */

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
    1.0e17,                 /* at boundary: shortcut excluded; matches json_encode scientific path */
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

/* Byte-for-byte parity with json_encode on values that take the long
 * shortcut on the current arch. On 64-bit, the shortcut covers
 * |d| < 1e17 to align with json_encode's fixed-notation cutoff
 * (json_encode switches to scientific at 1e17). With the bug, 1e10 /
 * 1e12 on 32-bit emitted INT32-saturated garbage; the fix routes them
 * to the REAL path. Values outside the per-arch shortcut bound go to
 * yyjson's REAL writer, which still appends ".0" -- json_encode
 * switches to scientific there, so a residual divergence remains for
 * |d| >= 1e17 and is intentional. */
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

/* Inside a container, exercises the same dw_emit_double path through
 * the encode-zval recursion. */
$out = fastjson_encode(['a' => 1.0e10, 'b' => -1.0e10]);
$back = fastjson_decode($out, true);
var_dump((float)$back['a'] == 1.0e10 && (float)$back['b'] == -1.0e10);
?>
--EXPECT--
roundtrip OK
shortcut parity OK
bool(true)
