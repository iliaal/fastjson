--TEST--
Bug #68817 (Null pointer deference)
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_decode('[""]'));

?>
--EXPECT--
array(1) {
  [0]=>
  string(0) ""
}
