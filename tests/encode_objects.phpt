--TEST--
fastjson_encode: stdClass, sparse arrays, FORCE_OBJECT
--EXTENSIONS--
fastjson
--FILE--
<?php

// stdClass -> JSON object.
$o = new stdClass();
$o->name = "ilia";
$o->n = 7;
var_dump(fastjson_encode($o));

// Empty stdClass.
var_dump(fastjson_encode(new stdClass()));

// Internal objects whose JSON property view is NULL are empty objects, not
// unsupported types.
var_dump(fastjson_encode(new WeakMap()));

// (object) cast of an array.
var_dump(fastjson_encode((object)["a" => 1, "b" => 2]));

echo "---\n";

// Sparse int-keyed arrays -> JSON object with stringified keys.
var_dump(fastjson_encode([0 => "a", 2 => "b"]));        // gap -> object
var_dump(fastjson_encode([5 => "x"]));                  // non-zero start -> object
var_dump(fastjson_encode(["1" => "a", 0 => "b"]));     // out-of-order -> object

echo "---\n";

// JSON_FORCE_OBJECT: even a perfect list becomes an object.
var_dump(fastjson_encode([1, 2, 3], JSON_FORCE_OBJECT));
var_dump(fastjson_encode([], JSON_FORCE_OBJECT));
?>
--EXPECT--
string(21) "{"name":"ilia","n":7}"
string(2) "{}"
string(2) "{}"
string(13) "{"a":1,"b":2}"
---
string(17) "{"0":"a","2":"b"}"
string(9) "{"5":"x"}"
string(17) "{"1":"a","0":"b"}"
---
string(19) "{"0":1,"1":2,"2":3}"
string(2) "{}"
