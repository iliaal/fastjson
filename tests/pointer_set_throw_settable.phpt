--TEST--
fastjson_pointer_set: JSON_THROW_ON_ERROR throws for non-settable mid-path
--EXTENSIONS--
fastjson
--FILE--
<?php

try {
    fastjson_pointer_set('{"a":1}', '/a/x', 1, 512, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (JsonException $e) {
    echo get_class($e), "\n";
    echo $e->getCode(), "\n";
    echo $e->getMessage(), "\n";
}

var_dump(fastjson_last_error() === JSON_ERROR_NONE);
?>
--EXPECT--
JsonException
4
JSON pointer does not resolve to a settable location
bool(true)
