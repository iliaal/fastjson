--TEST--
fastjson_file_decode drops the file buffer before building zvals
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

$rows = [];
for ($i = 0; $i < 8000; $i++) {
    $rows[] = ['id' => $i, 'name' => 'item-' . $i, 'on' => ($i & 1) === 0, 'note' => "a\\b"];
}
$json = json_encode($rows);
if (!is_string($json) || strlen($json) < 100000) {
    echo "payload too small\n";
    return;
}

$file = sys_get_temp_dir() . '/fastjson_file_decode_peak.json';
file_put_contents($file, $json);

gc_collect_cycles();
$before = memory_get_usage();
memory_reset_peak_usage();
$decoded = fastjson_decode($json, true);
$decodePeak = memory_get_peak_usage() - $before;
unset($decoded);

gc_collect_cycles();
$before = memory_get_usage();
memory_reset_peak_usage();
$fromFile = fastjson_file_decode($file, true);
$filePeak = memory_get_peak_usage() - $before;
unlink($file);

var_dump($fromFile === $rows);
var_dump(fastjson_last_error() === JSON_ERROR_NONE);
/* Holding the file bytes through the walk adds about one input copy.
 * Releasing them first keeps the extra under half the file size. */
var_dump($filePeak < $decodePeak + intdiv(strlen($json), 2));
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
