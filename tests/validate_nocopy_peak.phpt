--TEST--
fastjson_validate does not copy the input
--EXTENSIONS--
fastjson
--SKIPIF--
<?php
if (!function_exists('memory_reset_peak_usage')) {
    die('skip memory_reset_peak_usage requires PHP 8.2');
}
?>
--INI--
memory_limit=-1
--FILE--
<?php

$json = '[' . implode(',', array_fill(0, 8000, '{"id":1,"name":"item"}')) . ']';
$before = $json;
gc_collect_cycles();
$base = memory_get_usage();
memory_reset_peak_usage();
$ok = fastjson_validate($json);
$peak = memory_get_peak_usage() - $base;

var_dump($ok === true);
var_dump($json === $before);
var_dump(fastjson_last_error() === JSON_ERROR_NONE);
/* The old reader copied the whole document. Peak then tracked input size. */
var_dump($peak < intdiv(strlen($json), 2));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
