--TEST--
fastjson_decode() stops when its parse crosses memory_limit
--EXTENSIONS--
fastjson
--SKIPIF--
<?php require __DIR__ . '/memory_limit_skipif.inc'; ?>
--FILE--
<?php
require __DIR__ . '/memory_limit_child.inc';
/* yyjson's value pool doubles past the hard limit in one request. */
memory_limit_child('$j = "[" . str_repeat("{},", 6000000) . "{}]"; fastjson_decode($j);');
?>
--EXPECT--
OK
