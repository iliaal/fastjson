--TEST--
fastjson_last_error_info: structured error array shape on success and failure
--EXTENSIONS--
fastjson
--FILE--
<?php

// Failure: full structure with code/msg/pos/line/col.
fastjson_decode('{"a": bad}');
var_dump(fastjson_last_error_info());

// Success: code NONE, msg "No error", location cleared.
fastjson_decode('{"a":1}');
var_dump(fastjson_last_error_info());

// The array agrees with the scalar accessors.
fastjson_decode('[1,');
$i = fastjson_last_error_info();
var_dump(
    $i['code'] === fastjson_last_error(),
    $i['msg'] === fastjson_last_error_msg(),
    $i['pos'] === fastjson_last_error_pos()
);
?>
--EXPECT--
array(5) {
  ["code"]=>
  int(4)
  ["msg"]=>
  string(43) "unexpected character, expected a JSON value"
  ["pos"]=>
  int(6)
  ["line"]=>
  int(1)
  ["col"]=>
  int(7)
}
array(5) {
  ["code"]=>
  int(0)
  ["msg"]=>
  string(8) "No error"
  ["pos"]=>
  int(-1)
  ["line"]=>
  int(0)
  ["col"]=>
  int(0)
}
bool(true)
bool(true)
bool(true)
