--TEST--
fastjson_pointer_set: RFC 6901 single-value edit, re-emitted as JSON
--EXTENSIONS--
fastjson
--FILE--
<?php

$j = '{"a":1,"b":{"c":2}}';

// Replace a scalar; replace with an array; add a new object key.
echo fastjson_pointer_set($j, '/a', 99), "\n";
echo fastjson_pointer_set($j, '/b/c', [1, 2]), "\n";
echo fastjson_pointer_set($j, '/b/new', 'x'), "\n";

// Missing parent objects are created.
echo fastjson_pointer_set($j, '/newobj/deep', true), "\n";

// Empty pointer replaces the whole document.
echo fastjson_pointer_set($j, '', [9, 8]), "\n";

// Array index replacement.
echo fastjson_pointer_set('[1,2,3]', '/1', 50), "\n";

// Object value.
echo fastjson_pointer_set('{}', '/k', ['x' => 1]), "\n";

// A scalar mid-path is not settable: false + error, original untouched.
var_dump(fastjson_pointer_set($j, '/a/x', 1));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

// Output formatting follows the encode bits of $flags.
echo fastjson_pointer_set('{"a":1}', '/b', 2, 512, JSON_PRETTY_PRINT), "\n";
echo fastjson_pointer_set('{}', '/u', 'a/b'), "\n";
echo fastjson_pointer_set('{}', '/u', 'a/b', 512, JSON_UNESCAPED_SLASHES), "\n";

// Malformed non-empty pointers fail; they must not silently no-op.
var_dump(fastjson_pointer_set('{"a":1}', 'a', 2));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

// Parse error on the target: false + error, or throw under THROW_ON_ERROR.
var_dump(fastjson_pointer_set('{bad', '/x', 1));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
try {
    fastjson_pointer_set('{bad', '/x', 1, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo "threw: ", $e->getMessage(), "\n";
}

// $depth is argument #4.
try {
    fastjson_pointer_set('{}', '/x', 1, 0);
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
{"a":99,"b":{"c":2}}
{"a":1,"b":{"c":[1,2]}}
{"a":1,"b":{"c":2,"new":"x"}}
{"a":1,"b":{"c":2},"newobj":{"deep":true}}
[9,8]
[1,50,3]
{"k":{"x":1}}
bool(false)
bool(true)
{
    "a": 1,
    "b": 2
}
{"u":"a\/b"}
{"u":"a/b"}
bool(false)
bool(true)
bool(false)
bool(true)
threw: unexpected character, expected a string key
fastjson_pointer_set(): Argument #4 ($depth) must be greater than 0
