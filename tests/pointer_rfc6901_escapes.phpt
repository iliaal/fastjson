--TEST--
fastjson_pointer_get/_exists/_set: RFC 6901 escape handling
--EXTENSIONS--
fastjson
--FILE--
<?php

$json = '{"a/b":1,"m~n":2}';

var_dump(fastjson_pointer_get($json, '/a~1b'));
var_dump(fastjson_pointer_exists($json, '/m~0n'));
echo fastjson_pointer_set($json, '/a~1b', 9), "\n";
echo fastjson_pointer_set($json, '/m~0n', 8), "\n";

foreach (['/a~', '/a~2b'] as $ptr) {
    var_dump(fastjson_pointer_get($json, $ptr));
    var_dump(fastjson_pointer_exists($json, $ptr));
    var_dump(fastjson_pointer_set($json, $ptr, 1));
    var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
}
?>
--EXPECT--
int(1)
bool(true)
{"a\/b":9,"m~n":2}
{"a\/b":1,"m~n":8}
NULL
bool(false)
bool(false)
bool(true)
NULL
bool(false)
bool(false)
bool(true)
