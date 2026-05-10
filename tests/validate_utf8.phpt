--TEST--
fastjson_validate: invalid UTF-8 inside a JSON string maps to JSON_ERROR_UTF8
--EXTENSIONS--
fastjson
--FILE--
<?php

// Valid UTF-8 inside a string passes.
var_dump(fastjson_validate('"héllo"'));
var_dump(fastjson_validate('"é"'));

echo "---\n";

// Lone continuation byte (0x80) inside a JSON string -- yyjson rejects
// as INVALID_STRING; we surface that as JSON_ERROR_UTF8.
$bad = "\"" . "\x80" . "\"";
var_dump(fastjson_validate($bad));
var_dump(fastjson_last_error() === FASTJSON_ERROR_UTF8);
?>
--EXPECT--
bool(true)
bool(true)
---
bool(false)
bool(true)
