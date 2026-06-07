--TEST--
fastjson_decode: FASTJSON_DECODE_RELAXED tolerates the JSONC subset
--EXTENSIONS--
fastjson
--FILE--
<?php

// Comments (line + block) and trailing commas.
$jsonc = <<<'JSONC'
{
    // line comment
    "name": "Allan",   /* block comment */
    "roles": ["admin", "user",],
    "active": true,
}
JSONC;

var_dump(fastjson_decode($jsonc, true, 512, FASTJSON_DECODE_RELAXED));

// Leading UTF-8 BOM.
$bom = "\xEF\xBB\xBF" . '{"ok":true}';
var_dump(fastjson_decode($bom, true, 512, FASTJSON_DECODE_RELAXED));

// Without the flag, the same input is rejected (matches ext/json).
var_dump(fastjson_decode($jsonc, true));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

// Strict JSON still decodes identically with the flag set (no behavior
// change on well-formed input).
$strict = '{"a":1,"b":[2,3]}';
var_dump(
    fastjson_decode($strict, true, 512, FASTJSON_DECODE_RELAXED)
        === fastjson_decode($strict, true)
);
?>
--EXPECT--
array(3) {
  ["name"]=>
  string(5) "Allan"
  ["roles"]=>
  array(2) {
    [0]=>
    string(5) "admin"
    [1]=>
    string(4) "user"
  }
  ["active"]=>
  bool(true)
}
array(1) {
  ["ok"]=>
  bool(true)
}
NULL
bool(true)
bool(true)
