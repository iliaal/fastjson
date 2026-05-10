--TEST--
Bug #81532: Change of $depth behaviour in fastjson_encode() on PHP 8.1
--EXTENSIONS--
fastjson
--FILE--
<?php

// depth 1
$a = new \stdClass();

// depth 2
$b = new \stdClass();
$b->x = $a;

// depth 3
$c = new \stdClass();
$c->x = [$a];

var_export(fastjson_encode($a, 0, 0)); echo "\n";
var_export(fastjson_encode($a, 0, 1)); echo "\n";
var_export(fastjson_encode($b, 0, 1)); echo "\n";
var_export(fastjson_encode($b, 0, 2)); echo "\n";
var_export(fastjson_encode($c, 0, 2)); echo "\n";
var_export(fastjson_encode($c, 0, 3)); echo "\n";

?>
--EXPECT--
false
'{}'
false
'{"x":{}}'
false
'{"x":[{}]}'
