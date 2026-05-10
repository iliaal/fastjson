--TEST--
fastjson_encode() tests
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_encode(""));
var_dump(fastjson_encode(NULL));
var_dump(fastjson_encode(TRUE));

var_dump(fastjson_encode(array(""=>"")));
var_dump(fastjson_encode(array(array(1))));
var_dump(fastjson_encode(array()));

var_dump(fastjson_encode(array(""=>""), JSON_FORCE_OBJECT));
var_dump(fastjson_encode(array(array(1)), JSON_FORCE_OBJECT));
var_dump(fastjson_encode(array(), JSON_FORCE_OBJECT));

var_dump(fastjson_encode(1));
var_dump(fastjson_encode("руссиш"));

echo "Done\n";
?>
--EXPECT--
string(2) """"
string(4) "null"
string(4) "true"
string(7) "{"":""}"
string(5) "[[1]]"
string(2) "[]"
string(7) "{"":""}"
string(13) "{"0":{"0":1}}"
string(2) "{}"
string(1) "1"
string(38) ""\u0440\u0443\u0441\u0441\u0438\u0448""
Done
