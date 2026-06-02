--TEST--
fastjson_file_decode: an empty file decodes like fastjson_decode("") (not an I/O error)
--EXTENSIONS--
fastjson
--FILE--
<?php

$f = tempnam(sys_get_temp_dir(), 'fjempty');
file_put_contents($f, '');

// Non-throw: parity with fastjson_decode("") -- null, JSON_ERROR_SYNTAX,
// and the SAME error message (yyjson's "input length is 0"), NOT the
// I/O "Failed to read file".
$fileR = fastjson_file_decode($f);
$fileCode = fastjson_last_error();
$fileMsg = fastjson_last_error_msg();

$strR = fastjson_decode("");
$strCode = fastjson_last_error();
$strMsg = fastjson_last_error_msg();

var_dump($fileR === null);
var_dump($fileR === $strR);
var_dump($fileCode === JSON_ERROR_SYNTAX);
var_dump($fileCode === $strCode);
var_dump($fileMsg === $strMsg);

echo "---\n";

// Throw mode: an empty file throws JsonException, exactly like decode("").
$fileEx = null;
try { fastjson_file_decode($f, true, 512, JSON_THROW_ON_ERROR); }
catch (\JsonException $e) { $fileEx = $e->getMessage(); }

$strEx = null;
try { fastjson_decode("", true, 512, JSON_THROW_ON_ERROR); }
catch (\JsonException $e) { $strEx = $e->getMessage(); }

var_dump($fileEx !== null);
var_dump($fileEx === $strEx);

unlink($f);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
---
bool(true)
bool(true)
