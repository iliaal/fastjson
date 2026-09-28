--TEST--
decode aliases unescaped strings and leaves the caller buffer unchanged
--EXTENSIONS--
fastjson
json
--INI--
memory_limit=-1
--FILE--
<?php

$plain = '[' . implode(',', array_fill(0, 4000, '"abcdefghij"')) . ']';
$escaped = '[' . implode(',', array_fill(0, 4000, '"abcdefghi\\n"')) . ']';
$kept = $plain;

var_dump(fastjson_decode($plain) == json_decode($plain));
var_dump($plain === $kept);
var_dump(fastjson_decode($escaped) == json_decode($escaped));
var_dump(fastjson_decode('123') === 123);
var_dump(fastjson_decode('"é"') === 'é');
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
