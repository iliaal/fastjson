--TEST--
fastjson_merge_patch canonicalizes duplicate members without amplification
--EXTENSIONS--
fastjson
--FILE--
<?php

echo fastjson_encode(fastjson_merge_patch('{"a":1,"a":2}', '{}', true)), "\n";
echo fastjson_encode(fastjson_merge_patch('{}', '{"a":1,"a":2}', true)), "\n";
echo fastjson_encode(fastjson_merge_patch(
    '{"a":{"x":1},"a":{"x":2}}',
    '{"a":{"y":3}}',
    true
)), "\n";

$target = '{"a":{"blob":"' . str_repeat('x', 14000) . '"}}';
$patch = '{' . str_repeat('"a":{},', 5000) . '"a":2}';
$before = memory_get_usage(true);
$result = fastjson_merge_patch($target, $patch, true);
$peak = memory_get_peak_usage(true) - $before;
var_dump($result === ['a' => 2]);
var_dump($peak < 32 * 1024 * 1024);
?>
--EXPECT--
{"a":2}
{"a":2}
{"a":{"x":2,"y":3}}
bool(true)
bool(true)
