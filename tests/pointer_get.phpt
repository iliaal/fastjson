--TEST--
fastjson_pointer_get: RFC 6901 pointer resolution, not-found, errors
--EXTENSIONS--
fastjson
--FILE--
<?php

$j = '{"user":{"name":"Allan","roles":["admin","user"]},"n":42,"z":null}';

// Nested key, array index, scalar.
var_dump(fastjson_pointer_get($j, '/user/name'));
var_dump(fastjson_pointer_get($j, '/user/roles/1'));
var_dump(fastjson_pointer_get($j, '/n'));

// Subtree as object (default) vs associative array.
var_dump(fastjson_pointer_get($j, '/user'));
var_dump(fastjson_pointer_get($j, '/user', true));

// Empty pointer selects the whole document (RFC 6901).
var_dump(fastjson_pointer_get('[1,2,3]', '', true));

// Not found and malformed pointers return null without an error.
var_dump(fastjson_pointer_get($j, '/missing'));
var_dump(fastjson_pointer_get($j, '/user/roles/9'));
var_dump(fastjson_pointer_get($j, 'no-leading-slash'));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

// A pointer resolving to JSON null returns null too (indistinguishable).
var_dump(fastjson_pointer_get($j, '/z'));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

// Parse error: null + last_error set.
var_dump(fastjson_pointer_get('{bad', '/x'));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

// JSON_THROW_ON_ERROR throws on a parse error...
try {
    fastjson_pointer_get('{bad', '/x', null, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo "threw: ", $e->getMessage(), "\n";
}
// ...but a not-found pointer is not a JSON error, so it never throws.
var_dump(fastjson_pointer_get('{"a":1}', '/missing', null, 512, JSON_THROW_ON_ERROR));

// $depth is argument #4.
try {
    fastjson_pointer_get('{}', '', null, 0);
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
string(5) "Allan"
string(4) "user"
int(42)
object(stdClass)#1 (2) {
  ["name"]=>
  string(5) "Allan"
  ["roles"]=>
  array(2) {
    [0]=>
    string(5) "admin"
    [1]=>
    string(4) "user"
  }
}
array(2) {
  ["name"]=>
  string(5) "Allan"
  ["roles"]=>
  array(2) {
    [0]=>
    string(5) "admin"
    [1]=>
    string(4) "user"
  }
}
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
NULL
NULL
NULL
bool(true)
NULL
bool(true)
NULL
bool(true)
threw: unexpected character, expected a string key
NULL
fastjson_pointer_get(): Argument #4 ($depth) must be greater than 0
