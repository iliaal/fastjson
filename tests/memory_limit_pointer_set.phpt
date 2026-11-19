--TEST--
fastjson_pointer_set() stops at memory_limit while pretty-printing
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
/* Pretty-printing 200k values nested 300 deep writes ~1.2 KB of indent each. */
memory_limit_child('$j = "[" . str_repeat("[", 300) . "0" . str_repeat(",1", 200000) . str_repeat("]", 300) . ",0]"; fastjson_pointer_set($j, "/1", 2, 512, JSON_PRETTY_PRINT);');
?>
--EXPECT--
OK
