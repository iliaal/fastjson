--TEST--
fastjson_pointer_exists: presence check disambiguates absent from null
--EXTENSIONS--
fastjson
--FILE--
<?php

$j = '{"user":{"name":"Allan","roles":["admin"]},"z":null}';

// Present scalar, nested object, array index.
var_dump(fastjson_pointer_exists($j, '/user/name'));
var_dump(fastjson_pointer_exists($j, '/user'));
var_dump(fastjson_pointer_exists($j, '/user/roles/0'));

// The disambiguation win: a path resolving to JSON null is "present".
var_dump(fastjson_pointer_exists($j, '/z'));

// Empty pointer selects the whole document, which always exists.
var_dump(fastjson_pointer_exists($j, ''));

// Absent path / out-of-range index / malformed pointer: false, no error.
var_dump(fastjson_pointer_exists($j, '/user/missing'));
var_dump(fastjson_pointer_exists($j, '/user/roles/9'));
var_dump(fastjson_pointer_exists($j, 'no-slash'));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

// Parse error: false with last_error set.
var_dump(fastjson_pointer_exists('{bad', '/x'));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

// JSON_THROW_ON_ERROR throws on a parse error...
try {
    fastjson_pointer_exists('{bad', '/x', JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo "threw: ", $e->getMessage(), "\n";
}
// ...but an absent path is not a JSON error, so it never throws.
var_dump(fastjson_pointer_exists('{"a":1}', '/missing', JSON_THROW_ON_ERROR));

// RELAXED parsing applies to the input.
var_dump(fastjson_pointer_exists("{\"a\":1,}", '/a', FASTJSON_DECODE_RELAXED));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(false)
bool(false)
bool(false)
bool(true)
bool(false)
bool(true)
threw: unexpected character, expected a string key
bool(false)
bool(true)
