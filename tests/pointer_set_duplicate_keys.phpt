--TEST--
fastjson_pointer_set rejects ambiguous duplicate target members
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_pointer_set('{"a":1,"a":2}', '/a', 3));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
echo fastjson_last_error_msg(), "\n";

try {
    fastjson_pointer_set('{"a":{"x":1},"a":{"x":2}}', '/a/x', 3, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $exception) {
    echo $exception->getMessage(), "\n";
}
?>
--EXPECT--
bool(false)
bool(true)
JSON pointer target is ambiguous because the object contains duplicate members
JSON pointer target is ambiguous because the object contains duplicate members
