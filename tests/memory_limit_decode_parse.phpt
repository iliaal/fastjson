--TEST--
fastjson_decode() stops when its parse crosses memory_limit
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
/* yyjson's value pool doubles past the hard limit in one request. */
memory_limit_child('$j = "[" . str_repeat("{},", 6000000) . "{}]"; fastjson_decode($j);');
?>
--EXPECT--
OK
