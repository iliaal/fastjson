--TEST--
fastjson_decode() stops when building arrays crosses memory_limit
--EXTENSIONS--
fastjson
--SKIPIF--
<?php require __DIR__ . '/memory_limit_skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/memory_limit_child.inc';
memory_limit_child('$j = "[" . str_repeat("[0],", 900000) . "[0]]"; fastjson_decode($j, true);');
?>
--EXPECT--
OK
