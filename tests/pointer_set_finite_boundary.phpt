--TEST--
fastjson_pointer_set: untouched reals at the double overflow boundary
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Untouched reals below 10^308 skip strtod; the rest must still reject
 * values that overflow to INF. */
$cases = [
    '9.99999999999999999e307', '1.7976931348623157e308', '-1.7976931348623157E+308',
    '1.7976931348623159e308', '0.1e310', '1000e306', '0.00002e313', '1e308',
    '1e-400', '-0.0', '0.5e99999999999999999999', '5e-99999999999999999999',
];
foreach ($cases as $n) {
    $r = fastjson_pointer_set('{"x":' . $n . ',"y":2}', '/y', 3);
    echo $n, ' => ', $r === false ? fastjson_last_error_msg() : $r, "\n";
}
?>
--EXPECT--
9.99999999999999999e307 => {"x":9.99999999999999999e307,"y":3}
1.7976931348623157e308 => {"x":1.7976931348623157e308,"y":3}
-1.7976931348623157E+308 => {"x":-1.7976931348623157E+308,"y":3}
1.7976931348623159e308 => Inf and NaN cannot be JSON encoded
0.1e310 => Inf and NaN cannot be JSON encoded
1000e306 => Inf and NaN cannot be JSON encoded
0.00002e313 => Inf and NaN cannot be JSON encoded
1e308 => {"x":1e308,"y":3}
1e-400 => {"x":1e-400,"y":3}
-0.0 => {"x":-0.0,"y":3}
0.5e99999999999999999999 => Inf and NaN cannot be JSON encoded
5e-99999999999999999999 => {"x":5e-99999999999999999999,"y":3}
