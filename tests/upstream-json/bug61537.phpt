--TEST--
Bug #61537 (fastjson_encode() incorrectly truncates/discards information)
--EXTENSIONS--
fastjson
--FILE--
<?php
$invalid_utf8 = "\x9f";

var_dump(fastjson_encode($invalid_utf8));
var_dump(fastjson_last_error(), fastjson_last_error_msg());

var_dump(fastjson_encode($invalid_utf8, JSON_PARTIAL_OUTPUT_ON_ERROR));
var_dump(fastjson_last_error(), fastjson_last_error_msg());

echo "\n";

$invalid_utf8 = "an invalid sequen\xce in the middle of a string";

var_dump(fastjson_encode($invalid_utf8));
var_dump(fastjson_last_error(), fastjson_last_error_msg());

var_dump(fastjson_encode($invalid_utf8, JSON_PARTIAL_OUTPUT_ON_ERROR));
var_dump(fastjson_last_error(), fastjson_last_error_msg());

?>
--EXPECT--
bool(false)
int(5)
string(56) "Malformed UTF-8 characters, possibly incorrectly encoded"
string(4) "null"
int(5)
string(56) "Malformed UTF-8 characters, possibly incorrectly encoded"

bool(false)
int(5)
string(56) "Malformed UTF-8 characters, possibly incorrectly encoded"
string(4) "null"
int(5)
string(56) "Malformed UTF-8 characters, possibly incorrectly encoded"
