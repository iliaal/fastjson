--TEST--
Bug #68938 (fastjson_decode() decodes empty string without indicating error)
--EXTENSIONS--
fastjson
--FILE--
<?php
fastjson_decode("");
var_dump(fastjson_last_error());
?>
--EXPECT--
int(4)
