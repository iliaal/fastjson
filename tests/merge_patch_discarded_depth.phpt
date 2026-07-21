--TEST--
fastjson_merge_patch: depth applies to the effective result
--EXTENSIONS--
fastjson
--FILE--
<?php

function nestedJson(int $depth): string {
    return str_repeat('{"x":', $depth) . '0' . str_repeat('}', $depth);
}

$target = '{"deep":' . nestedJson(20) . ',"keep":1}';

var_dump(fastjson_merge_patch($target, '{"deep":null}', true, 5));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

var_dump(fastjson_merge_patch($target, '{"deep":0}', true, 5));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

var_dump(fastjson_merge_patch($target, '{"keep":2}', true, 5));
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);
?>
--EXPECT--
array(1) {
  ["keep"]=>
  int(1)
}
bool(true)
array(2) {
  ["deep"]=>
  int(0)
  ["keep"]=>
  int(1)
}
bool(true)
NULL
bool(true)
