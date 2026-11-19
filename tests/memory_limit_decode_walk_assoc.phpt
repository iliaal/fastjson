--TEST--
fastjson_decode() stops when building arrays crosses memory_limit
--EXTENSIONS--
fastjson
--SKIPIF--
<?php
if (!is_readable('/proc/self/maps')) print 'skip requires Linux procfs';
if (!function_exists('proc_open')) print 'skip proc_open unavailable';
?>
--FILE--
<?php
require __DIR__ . '/memory_limit_child.inc';
memory_limit_child('$j = "[" . str_repeat("[0],", 900000) . "[0]]"; fastjson_decode($j, true);');
?>
--EXPECT--
OK
