--TEST--
fastjson_decode: scalars, arrays, mixed nesting
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_decode('null'));
var_dump(fastjson_decode('true'));
var_dump(fastjson_decode('false'));
var_dump(fastjson_decode('0'));
var_dump(fastjson_decode('-7'));
var_dump(fastjson_decode('"hello"'));
var_dump(fastjson_decode('""'));

echo "---\n";

var_dump(fastjson_decode('[]'));
var_dump(fastjson_decode('[1, "two", true, null, 3.14]'));
var_dump(fastjson_decode('[[1, 2], [3, 4], []]'));

echo "---\n";

// Mixed nesting decoded as assoc.
$json = '{"users":[{"name":"a","age":1},{"name":"b","age":2}],"count":2}';
var_dump(fastjson_decode($json, true));

echo "---\n";

// fastjson_last_error reports NONE after success.
var_dump(fastjson_last_error());
var_dump(fastjson_last_error_msg());
?>
--EXPECT--
NULL
bool(true)
bool(false)
int(0)
int(-7)
string(5) "hello"
string(0) ""
---
array(0) {
}
array(5) {
  [0]=>
  int(1)
  [1]=>
  string(3) "two"
  [2]=>
  bool(true)
  [3]=>
  NULL
  [4]=>
  float(3.14)
}
array(3) {
  [0]=>
  array(2) {
    [0]=>
    int(1)
    [1]=>
    int(2)
  }
  [1]=>
  array(2) {
    [0]=>
    int(3)
    [1]=>
    int(4)
  }
  [2]=>
  array(0) {
  }
}
---
array(2) {
  ["users"]=>
  array(2) {
    [0]=>
    array(2) {
      ["name"]=>
      string(1) "a"
      ["age"]=>
      int(1)
    }
    [1]=>
    array(2) {
      ["name"]=>
      string(1) "b"
      ["age"]=>
      int(2)
    }
  }
  ["count"]=>
  int(2)
}
---
int(0)
string(8) "No error"
