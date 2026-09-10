--TEST--
fastjson_decode: $associative=true|false|null selects assoc array vs stdClass
--EXTENSIONS--
fastjson
--FILE--
<?php

$json = '{"a": 1, "b": 2}';

$default = fastjson_decode($json);
var_dump($default instanceof stdClass);
var_dump($default->a, $default->b);

$obj = fastjson_decode($json, false);
var_dump($obj instanceof stdClass);

$nullArg = fastjson_decode($json, null);
var_dump($nullArg instanceof stdClass);

$arr = fastjson_decode($json, true);
var_dump(is_array($arr));
var_dump($arr);

echo "---\n";

var_dump(fastjson_decode('{}'));
var_dump(fastjson_decode('{}', true));

echo "---\n";

var_dump(fastjson_decode('{"outer":{"inner":1}}', true));
?>
--EXPECTF--
bool(true)
int(1)
int(2)
bool(true)
bool(true)
bool(true)
array(2) {
  ["a"]=>
  int(1)
  ["b"]=>
  int(2)
}
---
object(stdClass)#%d (0) {
}
array(0) {
}
---
array(1) {
  ["outer"]=>
  array(1) {
    ["inner"]=>
    int(1)
  }
}
