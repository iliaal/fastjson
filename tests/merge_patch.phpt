--TEST--
fastjson_merge_patch: RFC 7386 JSON Merge Patch semantics
--EXTENSIONS--
fastjson
--FILE--
<?php

// Recursive object merge: overlay wins, null deletes, new keys added.
var_dump(fastjson_merge_patch(
    '{"a":1,"b":{"x":1,"y":2}}',
    '{"b":{"y":null,"z":9},"c":3}',
    true
));

// A non-object patch replaces the target wholesale.
var_dump(fastjson_merge_patch('{"a":1}', '"replaced"'));
var_dump(fastjson_merge_patch('{"a":1}', '[1,2]', true));

// Null patch replaces the whole target with null.
var_dump(fastjson_merge_patch('{"a":1}', 'null'));

// Object output (default) vs associative.
var_dump(fastjson_merge_patch('{"a":1}', '{"b":2}'));

// Result is a PHP value; re-encode for a string.
echo fastjson_encode(fastjson_merge_patch('{"a":1}', '{"b":2}', true)), "\n";

// Parse error in either operand: null + last_error set.
var_dump(fastjson_merge_patch('{bad', '{}'));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
var_dump(fastjson_merge_patch('{}', '{bad'));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

// JSON_THROW_ON_ERROR throws on a parse error.
try {
    fastjson_merge_patch('{bad', '{}', null, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo "threw: ", $e->getMessage(), "\n";
}

// $flags (FASTJSON_DECODE_RELAXED) applies to operand parsing.
var_dump(fastjson_merge_patch('{"a":1,}', '{"b":2}', true, 512, FASTJSON_DECODE_RELAXED));

// $depth is argument #4.
try {
    fastjson_merge_patch('{}', '{}', null, 0);
} catch (ValueError $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
array(3) {
  ["a"]=>
  int(1)
  ["b"]=>
  array(2) {
    ["x"]=>
    int(1)
    ["z"]=>
    int(9)
  }
  ["c"]=>
  int(3)
}
string(8) "replaced"
array(2) {
  [0]=>
  int(1)
  [1]=>
  int(2)
}
NULL
object(stdClass)#1 (2) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
}
{"a":1,"b":2}
NULL
bool(true)
NULL
bool(true)
threw: unexpected character, expected a string key
array(2) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
}
fastjson_merge_patch(): Argument #4 ($depth) must be greater than 0
