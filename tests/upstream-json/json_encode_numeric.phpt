--TEST--
Test fastjson_encode() function with numeric flag
--EXTENSIONS--
fastjson
--INI--
serialize_precision=-1
--FILE--
<?php
var_dump(
    fastjson_encode("1", JSON_NUMERIC_CHECK),
    fastjson_encode("9.4324", JSON_NUMERIC_CHECK),
    fastjson_encode(array("122321", "3232595.33423"), JSON_NUMERIC_CHECK),
    fastjson_encode("1"),
    fastjson_encode("9.4324"),
    fastjson_encode(array("122321", "3232595.33423"))
);
?>
--EXPECT--
string(1) "1"
string(6) "9.4324"
string(22) "[122321,3232595.33423]"
string(3) ""1""
string(8) ""9.4324""
string(26) "["122321","3232595.33423"]"
