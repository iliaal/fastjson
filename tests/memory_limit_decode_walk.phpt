--TEST--
fastjson_decode() stops when building objects crosses memory_limit
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
/* The parse fits; building 1.2M stdClass objects does not. */
memory_limit_child('$j = "[" . str_repeat("{},", 1200000) . "{}]"; fastjson_decode($j);');
?>
--EXPECT--
OK
