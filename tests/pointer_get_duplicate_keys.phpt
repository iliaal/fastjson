--TEST--
fastjson_pointer_get and exists: duplicate members on the path are ambiguous
--EXTENSIONS--
fastjson
--FILE--
<?php

$json = '{"role":"user","role":"admin"}';
var_dump(fastjson_pointer_get($json, '/role'));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
echo fastjson_last_error_msg(), "\n";

var_dump(fastjson_pointer_exists($json, '/role'));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

$nested = '{"account":{"role":"user"},"account":{"role":"admin"}}';
var_dump(fastjson_pointer_get($nested, '/account/role'));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

$unrelated = '{"wanted":1,"other":{"x":1,"x":2}}';
var_dump(fastjson_pointer_get($unrelated, '/wanted'));
var_dump(fastjson_pointer_exists($unrelated, '/wanted'));

try {
    fastjson_pointer_get($json, '/role', null, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo $e->getCode(), ': ', $e->getMessage(), "\n";
}
?>
--EXPECT--
NULL
bool(true)
JSON pointer target is ambiguous because the object contains duplicate members
bool(false)
bool(true)
NULL
bool(true)
int(1)
bool(true)
4: JSON pointer target is ambiguous because the object contains duplicate members
