--TEST--
fastjson_decode: int / float / large uint > LONG_MAX widens to double
--EXTENSIONS--
fastjson
--FILE--
<?php

// Signed int range.
var_dump(fastjson_decode('0'));
var_dump(fastjson_decode('-1'));
var_dump(fastjson_decode('9223372036854775807'));   // PHP_INT_MAX (LP64)
var_dump(fastjson_decode('-9223372036854775808'));  // PHP_INT_MIN

echo "---\n";

// Unsigned overflow -> double.
$big = fastjson_decode('9223372036854775808');      // PHP_INT_MAX + 1
var_dump(is_float($big));
var_dump($big > 9.22e18);

$max64 = fastjson_decode('18446744073709551615');   // 2^64 - 1
var_dump(is_float($max64));

echo "---\n";

// Floats.
var_dump(fastjson_decode('3.14'));
var_dump(fastjson_decode('-0.5'));
var_dump(fastjson_decode('1e10'));
var_dump(fastjson_decode('1.5e-3'));
?>
--EXPECT--
int(0)
int(-1)
int(9223372036854775807)
int(-9223372036854775808)
---
bool(true)
bool(true)
bool(true)
---
float(3.14)
float(-0.5)
float(10000000000)
float(0.0015)
