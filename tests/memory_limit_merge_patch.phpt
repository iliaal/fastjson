--TEST--
fastjson_merge_patch() stops at memory_limit
--EXTENSIONS--
fastjson
--SKIPIF--
<?php require __DIR__ . '/memory_limit_skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/memory_limit_child.inc';
memory_limit_child('$j = "[" . str_repeat("{},", 1200000) . "{}]"; fastjson_merge_patch("{}", "{\\"a\\":$j}");');
?>
--EXPECT--
OK
