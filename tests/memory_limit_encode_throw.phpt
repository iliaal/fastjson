--TEST--
fastjson_encode() with JSON_THROW_ON_ERROR reports the memory-limit fatal error, not a JsonException
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
memory_limit_child('try { fastjson_encode(array_fill(0, 200, str_repeat("a", 1 << 20)), JSON_THROW_ON_ERROR); } catch (Throwable $e) { echo "caught\\n"; }');
?>
--EXPECT--
OK
