--TEST--
Bug #41567 (fastjson_encode() double conversion is inconsistent with PHP)
--EXTENSIONS--
fastjson
--INI--
serialize_precision=-1
--FILE--
<?php

$a = fastjson_encode(123456789.12345);
var_dump(fastjson_decode($a));

echo "Done\n";
?>
--EXPECT--
float(123456789.12345)
Done
