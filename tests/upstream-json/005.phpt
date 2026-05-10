--TEST--
fastjson_encode() & endless loop - 3
--EXTENSIONS--
fastjson
--FILE--
<?php

$a = array();
$a[] = $a;

var_dump($a);
var_dump(fastjson_encode($a));

echo "Done\n";
?>
--EXPECT--
array(1) {
  [0]=>
  array(0) {
  }
}
string(4) "[[]]"
Done
