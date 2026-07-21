--TEST--
fastjson_pointer_set: depth applies to the effective result
--EXTENSIONS--
fastjson
--FILE--
<?php

function nestedJson(int $depth): string {
    return str_repeat('{"x":', $depth) . '0' . str_repeat('}', $depth);
}

$base = '{"deep":' . nestedJson(20) . ',"keep":1}';

echo fastjson_pointer_set($base, '/deep', 0, 5), "\n";
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

var_dump(fastjson_pointer_set($base, '/keep', 2, 5));
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);

echo fastjson_pointer_set($base, '', 0, 1), "\n";
?>
--EXPECT--
{"deep":0,"keep":1}
bool(true)
bool(false)
bool(true)
0
