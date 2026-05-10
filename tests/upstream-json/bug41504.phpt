--TEST--
Bug #41504 (fastjson_decode() converts empty array keys to "_empty_")
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_decode('{"":"value"}', true));
var_dump(fastjson_decode('{"":"value", "key":"value"}', true));
var_dump(fastjson_decode('{"key":"value", "":"value"}', true));

echo "Done\n";
?>
--EXPECT--
array(1) {
  [""]=>
  string(5) "value"
}
array(2) {
  [""]=>
  string(5) "value"
  ["key"]=>
  string(5) "value"
}
array(2) {
  ["key"]=>
  string(5) "value"
  [""]=>
  string(5) "value"
}
Done
