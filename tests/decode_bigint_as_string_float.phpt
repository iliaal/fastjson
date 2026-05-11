--TEST--
fastjson_decode: JSON_BIGINT_AS_STRING applies to integer overflow only, not float overflow
--EXTENSIONS--
fastjson
--FILE--
<?php

// ext/json's JSON_BIGINT_AS_STRING is documented as "encodes large
// integers as their original string value" -- floats are not in scope.
// Exponent-overflow numbers go to INF, not to the raw text.

// Integer overflow -> string (BIGINT_AS_STRING applies).
$r = fastjson_decode('999999999999999999999999', false, 512, JSON_BIGINT_AS_STRING);
var_dump($r);
var_dump($r === json_decode('999999999999999999999999', false, 512, JSON_BIGINT_AS_STRING));

// Float overflow -> INF (BIGINT_AS_STRING does NOT apply).
$r = fastjson_decode('1e309', false, 512, JSON_BIGINT_AS_STRING);
var_dump($r);
$ext = json_decode('1e309', false, 512, JSON_BIGINT_AS_STRING);
var_dump($ext);
var_dump($r === $ext);

// Negative float overflow.
var_dump(fastjson_decode('-1e309', false, 512, JSON_BIGINT_AS_STRING));

// Inside a container.
$r = fastjson_decode('[999999999999999999999999, 1e309, 42]', true, 512, JSON_BIGINT_AS_STRING);
var_dump($r);

// Regular doubles still decode to doubles regardless of the flag.
var_dump(fastjson_decode('1.5', false, 512, JSON_BIGINT_AS_STRING));
var_dump(fastjson_decode('1e10', false, 512, JSON_BIGINT_AS_STRING));
?>
--EXPECT--
string(24) "999999999999999999999999"
bool(true)
float(INF)
float(INF)
bool(true)
float(-INF)
array(3) {
  [0]=>
  string(24) "999999999999999999999999"
  [1]=>
  float(INF)
  [2]=>
  int(42)
}
float(1.5)
float(10000000000)
