--TEST--
fastjson_decode: object key handling for "0" and "" across assoc/object modes
--EXTENSIONS--
fastjson
--FILE--
<?php

// Numeric-string keys.
// Object mode: keys stay as strings.
$obj = fastjson_decode('{"0": "a", "1": "b"}');
var_dump($obj);
var_dump($obj->{'0'});
var_dump($obj->{'1'});

// Assoc mode: PHP standard "0" -> 0 numeric promotion.
$arr = fastjson_decode('{"0": "a", "1": "b"}', true);
var_dump($arr);
var_dump(array_keys($arr));   // expect [0, 1] as ints

echo "---\n";

// Empty key.
$obj = fastjson_decode('{"": "empty key"}');
var_dump($obj->{''});

$arr = fastjson_decode('{"": "empty key"}', true);
var_dump($arr);
var_dump($arr['']);

echo "---\n";

// Duplicate key: last wins (yyjson iter yields all; zend_hash_update overwrites).
$arr = fastjson_decode('{"k": 1, "k": 2}', true);
var_dump($arr);

$obj = fastjson_decode('{"k": 1, "k": 2}');
var_dump($obj->k);
?>
--EXPECT--
object(stdClass)#1 (2) {
  ["0"]=>
  string(1) "a"
  ["1"]=>
  string(1) "b"
}
string(1) "a"
string(1) "b"
array(2) {
  [0]=>
  string(1) "a"
  [1]=>
  string(1) "b"
}
array(2) {
  [0]=>
  int(0)
  [1]=>
  int(1)
}
---
string(9) "empty key"
array(1) {
  [""]=>
  string(9) "empty key"
}
string(9) "empty key"
---
array(1) {
  ["k"]=>
  int(2)
}
int(2)
