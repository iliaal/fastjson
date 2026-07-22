--TEST--
fastjson_encode: later errors take precedence over earlier INF and NAN
--EXTENSIONS--
fastjson
--FILE--
<?php

$resource = fopen('php://memory', 'r');

$cases = [
    [[INF, $resource], 512, 0],
    [[NAN, $resource], 512, 0],
    [[INF, [[1]]], 2, 0],
    [['first' => INF, 'later' => $resource], 512, 0],
    [(object)['first' => INF, 'later' => $resource], 512, 0],
    [[INF, $resource], 512, JSON_FORCE_OBJECT],
    [[INF, "\xFF"], 512, 0],
    [[INF, "\xFF"], 512, JSON_INVALID_UTF8_IGNORE],
    [['first' => INF, "\xFF" => 1], 512, 0],
];

foreach ($cases as [$value, $depth, $flags]) {
    json_encode($value, $flags, $depth);
    $native = json_last_error();
    var_dump(fastjson_encode($value, $flags, $depth));
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
bool(false)
bool(true)
8
bool(false)
bool(true)
8
bool(false)
bool(true)
8
bool(false)
bool(true)
5
bool(false)
bool(true)
7
bool(false)
bool(true)
5
string(8) "[0,null]"
bool(true)
