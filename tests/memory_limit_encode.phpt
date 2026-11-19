--TEST--
fastjson_encode() stops at memory_limit
--EXTENSIONS--
fastjson
--SKIPIF--
<?php require __DIR__ . '/memory_limit_skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/memory_limit_child.inc';
/* 200 references to one 1 MiB string encode to 200 MiB. */
memory_limit_child('$s = str_repeat("a", 1 << 20); fastjson_encode(array_fill(0, 200, $s));');
?>
--EXPECT--
OK
