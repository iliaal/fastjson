--TEST--
Bug #45791 (fastjson_decode() does not handle number 0e0)
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_decode('{"zero": 0e0}'));

?>
--EXPECT--
object(stdClass)#1 (1) {
  ["zero"]=>
  float(0)
}
