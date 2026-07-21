--TEST--
fastjson_encode: later errors take precedence over earlier INF and NAN
--EXTENSIONS--
fastjson
--FILE--
<?php

$resource = fopen('php://memory', 'r');

$cases = [
    [[INF, $resource], 512],
    [[NAN, $resource], 512],
    [[INF, [[1]]], 2],
];

foreach ($cases as [$value, $depth]) {
    json_encode($value, 0, $depth);
    $native = json_last_error();
    var_dump(fastjson_encode($value, 0, $depth));
    var_dump(fastjson_last_error() === $native);
    echo $native, "\n";
}

var_dump(fastjson_encode([INF, $resource], JSON_PARTIAL_OUTPUT_ON_ERROR));
var_dump(fastjson_last_error() === JSON_ERROR_UNSUPPORTED_TYPE);

fclose($resource);
?>
--EXPECT--
bool(false)
bool(true)
8
bool(false)
bool(true)
8
bool(false)
bool(true)
1
string(8) "[0,null]"
bool(true)
