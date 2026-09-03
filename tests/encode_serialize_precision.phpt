--TEST--
fastjson_encode honors serialize_precision like ext/json for doubles and numeric-check strings
--EXTENSIONS--
fastjson
--FILE--
<?php
$doubles = [1.5, 100.0, 1e16, 123456789.123456789, 0.1 + 0.2, 1.0e-7, 1230.0, -0.0];

foreach ([5, 10, 17] as $precision) {
    ini_set('serialize_precision', (string) $precision);
    foreach ($doubles as $d) {
        var_dump(fastjson_encode($d) === json_encode($d));
    }
    var_dump(fastjson_encode($doubles) === json_encode($doubles));
    var_dump(fastjson_encode('3.14159265358979', JSON_NUMERIC_CHECK) === json_encode('3.14159265358979', JSON_NUMERIC_CHECK));
    var_dump(fastjson_encode(100.0, JSON_PRESERVE_ZERO_FRACTION) === json_encode(100.0, JSON_PRESERVE_ZERO_FRACTION));
}

ini_set('serialize_precision', '-1');
var_dump(fastjson_encode(1e16) === json_encode(1e16));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
