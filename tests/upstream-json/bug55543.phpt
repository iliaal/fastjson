--TEST--
Bug #55543 (fastjson_encode() with JSON_NUMERIC_CHECK & numeric string properties)
--EXTENSIONS--
fastjson
--FILE--
<?php
$a = new stdClass;
$a->{"1"} = "5";

var_dump(fastjson_encode($a, JSON_NUMERIC_CHECK));
?>
--EXPECT--
string(7) "{"1":5}"
