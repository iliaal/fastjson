--TEST--
fastjson_decode() with JSON_INVALID_UTF8_SUBSTITUTE stops at memory_limit
--EXTENSIONS--
fastjson
--SKIPIF--
<?php require __DIR__ . '/memory_limit_skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/memory_limit_child.inc';
/* SUBSTITUTE reserves three bytes per input byte, then copies the result. */
memory_limit_child('$j = "\\"" . str_repeat("\\xff", 18000000) . "\\""; fastjson_decode($j, false, 512, JSON_INVALID_UTF8_SUBSTITUTE);');
?>
--EXPECT--
OK
