--TEST--
fastjson_encode and pointer_set bound large-string reserve memory
--EXTENSIONS--
fastjson
--FILE--
<?php

$plain = str_repeat('a', 4 * 1024 * 1024);
$before = memory_get_usage(true);
$encoded = fastjson_encode($plain);
$encodePeak = memory_get_peak_usage(true) - $before;
var_dump(strlen($encoded) === strlen($plain) + 2);
var_dump($encodePeak < 16 * 1024 * 1024);

$special = str_repeat("a/\\\"\n\u{20AC}", 140000);
foreach ([0, JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE] as $flags) {
    var_dump(fastjson_encode($special, $flags) === json_encode($special, $flags));
}

$doc = '{"blob":"' . $plain . '","x":1}';
$before = memory_get_usage(true);
$updated = fastjson_pointer_set($doc, '/x', 2);
$pointerPeak = memory_get_peak_usage(true) - $before;
var_dump(strlen($updated) === strlen($doc));
var_dump($pointerPeak < 32 * 1024 * 1024);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
