--TEST--
fastjson_encode() & endless loop - 2
--EXTENSIONS--
fastjson
--FILE--
<?php

$a = new stdclass;
$a->prop = $a;

var_dump($a);

echo "\n";

var_dump(fastjson_encode($a));
var_dump(fastjson_last_error(), fastjson_last_error_msg());

echo "\n";

var_dump(fastjson_encode($a, JSON_PARTIAL_OUTPUT_ON_ERROR));
var_dump(fastjson_last_error(), fastjson_last_error_msg());

echo "Done\n";
?>
--EXPECTF--
object(stdClass)#%d (1) {
  ["prop"]=>
  *RECURSION*
}

bool(false)
int(6)
string(%d) "Recursion detected"

string(13) "{"prop":null}"
int(6)
string(%d) "Recursion detected"
Done
