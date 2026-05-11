--TEST--
fastjson_decode: JSON_OBJECT_AS_ARRAY in $flags pivots when $associative is null
--EXTENSIONS--
fastjson
--FILE--
<?php

$j = '{"a":1,"b":[2,3]}';

// $associative=null + flag set: arrays.
$r = fastjson_decode($j, null, 512, JSON_OBJECT_AS_ARRAY);
var_dump(is_array($r));
var_dump($r);

// $associative=null + no flag: stdClass (default).
$r = fastjson_decode($j);
var_dump($r instanceof stdClass);

// Explicit $associative=false WINS over the flag (ext/json contract).
$r = fastjson_decode($j, false, 512, JSON_OBJECT_AS_ARRAY);
var_dump($r instanceof stdClass);

// Explicit $associative=true is the same as the flag.
$r = fastjson_decode($j, true);
var_dump(is_array($r));

// ext/json parity check.
var_dump(
    fastjson_decode($j, null, 512, JSON_OBJECT_AS_ARRAY)
        == json_decode($j, null, 512, JSON_OBJECT_AS_ARRAY)
);
?>
--EXPECT--
bool(true)
array(2) {
  ["a"]=>
  int(1)
  ["b"]=>
  array(2) {
    [0]=>
    int(2)
    [1]=>
    int(3)
  }
}
bool(true)
bool(true)
bool(true)
bool(true)
