--TEST--
an input that is only a UTF-8 BOM is empty under FASTJSON_DECODE_RELAXED
--EXTENSIONS--
fastjson
--FILE--
<?php

foreach (["\xEF\xBB\xBF", "\xEF\xBB\xBF  ", "\xEF\xBB\xBF\n"] as $input) {
    var_dump(fastjson_decode($input, true, 512, FASTJSON_DECODE_RELAXED));
    var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
    var_dump(fastjson_last_error_msg(), fastjson_last_error_pos());
}
var_dump(fastjson_decode("\xEF\xBB\xBF[1]", true, 512, FASTJSON_DECODE_RELAXED));
?>
--EXPECT--
NULL
bool(true)
string(19) "input data is empty"
int(0)
NULL
bool(true)
string(19) "input data is empty"
int(0)
NULL
bool(true)
string(19) "input data is empty"
int(0)
array(1) {
  [0]=>
  int(1)
}
