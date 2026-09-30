--TEST--
encode does not keep a 6x reserve or a 4KB page for small strings
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

function peak(string $value): int {
    gc_collect_cycles();
    $before = memory_get_usage();
    memory_reset_peak_usage();
    $encoded = fastjson_encode($value);
    $delta = memory_get_peak_usage() - $before;
    if ($encoded !== json_encode($value)) {
        echo "mismatch\n";
    }
    return $delta;
}

$small = str_repeat('a', 40);
$escaped = "\"\n" . str_repeat('b', 20000);

$smallPeak = peak($small);
$escapedPeak = peak($escaped);

var_dump($smallPeak < 1024);
var_dump($escapedPeak < 4 * strlen($escaped));
?>
--EXPECT--
bool(true)
bool(true)
