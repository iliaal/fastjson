--TEST--
an unescaped document uses less decode peak than the same document with an escape
--EXTENSIONS--
fastjson
--SKIPIF--
<?php
if (!function_exists('memory_reset_peak_usage')) {
    die('skip memory_reset_peak_usage requires PHP 8.2');
}
$before = memory_get_usage();
memory_reset_peak_usage();
$blob = str_repeat('a', 200000);
if (memory_get_peak_usage() <= $before + 10000) {
    die('skip allocator does not record peak usage');
}
?>
--INI--
memory_limit=-1
--FILE--
<?php

function peak(string $json): int {
    gc_collect_cycles();
    $before = memory_get_usage();
    memory_reset_peak_usage();
    $value = fastjson_decode($json);
    $delta = memory_get_peak_usage() - $before;
    unset($value);
    return max(0, $delta);
}

$plain = '[' . implode(',', array_fill(0, 4000, '"abcdefghij"')) . ']';
$escaped = '[' . implode(',', array_fill(0, 4000, '"abcdefghi\\n"')) . ']';
var_dump(peak($plain) + intdiv(strlen($plain), 2) < peak($escaped));
?>
--EXPECT--
bool(true)
