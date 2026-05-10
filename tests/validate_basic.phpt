--TEST--
fastjson_validate: accepts well-formed JSON, rejects malformed input
--EXTENSIONS--
fastjson
--FILE--
<?php

// Valid cases.
var_dump(fastjson_validate('null'));
var_dump(fastjson_validate('true'));
var_dump(fastjson_validate('false'));
var_dump(fastjson_validate('42'));
var_dump(fastjson_validate('"hello"'));
var_dump(fastjson_validate('[]'));
var_dump(fastjson_validate('{}'));
var_dump(fastjson_validate('[1, 2, 3]'));
var_dump(fastjson_validate('{"a": 1, "b": [true, null]}'));

echo "---\n";

// Invalid cases.
var_dump(fastjson_validate(''));                  // empty
var_dump(fastjson_validate('{'));                 // unterminated
var_dump(fastjson_validate('[1,]'));              // trailing comma
var_dump(fastjson_validate('{"a":}'));            // missing value
var_dump(fastjson_validate('{a:1}'));             // unquoted key
var_dump(fastjson_validate('undefined'));         // bare identifier
var_dump(fastjson_validate('[1, 2, 3] garbage')); // trailing content
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
---
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
