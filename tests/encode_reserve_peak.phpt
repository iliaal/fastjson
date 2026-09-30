--TEST--
encode writes long escaped strings without a 6x reserve
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

$escaped = "\"\n" . str_repeat('b', 20000);

$chunked = "\"\n" . str_repeat('b', 8190);

$escapedPeak = peak($escaped);
$chunkedPeak = peak($chunked);

var_dump($escapedPeak < 4 * strlen($escaped));
/* 8 KiB is the first chunked length: the input plus 12 KiB of chunk
 * headroom, where a 6x reserve took 52 KiB. */
var_dump($chunkedPeak < 40 * 1024);
?>
--EXPECT--
bool(true)
bool(true)
