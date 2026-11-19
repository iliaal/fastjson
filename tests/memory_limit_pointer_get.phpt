--TEST--
fastjson_pointer_get() stops when building the result crosses memory_limit
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
memory_limit_child('$j = "{\\"a\\":[" . str_repeat("{},", 1200000) . "{}]}"; fastjson_pointer_get($j, "/a");');
?>
--EXPECT--
OK
