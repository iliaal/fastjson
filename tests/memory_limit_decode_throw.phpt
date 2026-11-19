--TEST--
fastjson_decode() with JSON_THROW_ON_ERROR reports the memory-limit fatal error, not a JsonException
--EXTENSIONS--
fastjson
--SKIPIF--
<?php require __DIR__ . '/memory_limit_skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/memory_limit_child.inc';
memory_limit_child('try { $j = "[" . str_repeat("{},", 1200000) . "{}]"; fastjson_decode($j, false, 512, JSON_THROW_ON_ERROR); } catch (Throwable $e) { echo "caught\\n"; }');
?>
--EXPECT--
OK
