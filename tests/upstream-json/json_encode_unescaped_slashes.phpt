--TEST--
fastjson_decode() tests
--EXTENSIONS--
fastjson
--FILE--
<?php
var_dump(fastjson_encode('a/b'));
var_dump(fastjson_encode('a/b', JSON_UNESCAPED_SLASHES));
?>
--EXPECT--
string(6) ""a\/b""
string(5) ""a/b""
