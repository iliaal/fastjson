--TEST--
fastjson_file_decode: reads + decodes a file; assoc/object modes, depth, flags
--EXTENSIONS--
fastjson
--FILE--
<?php

$file = sys_get_temp_dir() . '/fastjson_file_decode_basic.json';
file_put_contents($file, '{"a":1,"b":[2,3],"big":99999999999999999999}');

// Default: stdClass.
$obj = fastjson_file_decode($file);
var_dump($obj instanceof stdClass);
var_dump($obj->a, $obj->b);
var_dump(fastjson_last_error());

// Associative array.
$arr = fastjson_file_decode($file, true);
var_dump($arr['a'], $arr['b']);

// Flags carry through: JSON_BIGINT_AS_STRING keeps the oversized int as string.
$big = fastjson_file_decode($file, true, 512, JSON_BIGINT_AS_STRING);
var_dump($big['big']);

echo "---\n";

// Depth is enforced exactly like fastjson_decode (decode counts every level).
file_put_contents($file, '[[[1]]]');
var_dump(fastjson_file_decode($file, true, 3));        // needs depth 4 => null
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);
var_dump(fastjson_file_decode($file, true, 4));        // ok

unlink($file);
?>
--EXPECT--
bool(true)
int(1)
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
int(0)
int(1)
array(2) {
  [0]=>
  int(2)
  [1]=>
  int(3)
}
string(20) "99999999999999999999"
---
NULL
bool(true)
array(1) {
  [0]=>
  array(1) {
    [0]=>
    array(1) {
      [0]=>
      int(1)
    }
  }
}
