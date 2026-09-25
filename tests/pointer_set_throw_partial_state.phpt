--TEST--
fastjson_pointer_set: throw-mode partial output publishes replacement errors
--EXTENSIONS--
fastjson
--FILE--
<?php

fastjson_decode('{"a": bad}');
$before = fastjson_last_error();
$result = fastjson_pointer_set(
    '{}',
    '/x',
    INF,
    512,
    JSON_THROW_ON_ERROR | JSON_PARTIAL_OUTPUT_ON_ERROR,
);
var_dump($result);
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);
var_dump(fastjson_last_error() === $before);
?>
--EXPECT--
string(7) "{"x":0}"
bool(true)
bool(false)
