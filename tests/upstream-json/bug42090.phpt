--TEST--
Bug #42090 (fastjson_decode causes segmentation fault)
--EXTENSIONS--
fastjson
--FILE--
<?php
var_dump(
    fastjson_decode('""'),
    fastjson_decode('"..".'),
    fastjson_decode('"'),
    fastjson_decode('""""'),
    fastjson_encode('"'),
    fastjson_decode(fastjson_encode('"')),
    fastjson_decode(fastjson_encode('""'))
);
?>
--EXPECT--
string(0) ""
NULL
NULL
NULL
string(4) ""\"""
string(1) """
string(2) """"
