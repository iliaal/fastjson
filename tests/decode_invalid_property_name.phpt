--TEST--
fastjson_decode rejects NUL-prefixed stdClass properties but preserves assoc keys
--EXTENSIONS--
fastjson
--FILE--
<?php

$json = '{"\\u0000key":1}';
var_dump(fastjson_decode($json));
var_dump(fastjson_last_error() === JSON_ERROR_INVALID_PROPERTY_NAME);
echo fastjson_last_error_msg(), "\n";

$assoc = fastjson_decode($json, true);
var_dump(count($assoc));
var_dump(array_key_exists("\0key", $assoc));
var_dump($assoc["\0key"]);

try {
    fastjson_decode($json, false, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $exception) {
    echo $exception->getCode(), ': ', $exception->getMessage(), "\n";
}
?>
--EXPECT--
NULL
bool(true)
The decoded property name is invalid
int(1)
bool(true)
int(1)
9: The decoded property name is invalid
