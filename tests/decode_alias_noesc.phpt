--TEST--
decode aliases unescaped strings and leaves the caller buffer unchanged
--EXTENSIONS--
fastjson
json
--INI--
memory_limit=-1
--FILE--
<?php

$plain = '[' . implode(',', array_fill(0, 4000, '"abcdefghij"')) . ']';
$escaped = '[' . implode(',', array_fill(0, 4000, '"abcdefghi\\n"')) . ']';
$kept = $plain;

var_dump(fastjson_decode($plain) == json_decode($plain));
var_dump($plain === $kept);
var_dump(fastjson_decode($escaped) == json_decode($escaped));
var_dump(fastjson_decode('123') === 123);
var_dump(fastjson_decode('"é"') === 'é');

function peak(string $json): int {
    gc_collect_cycles();
    $before = memory_get_usage();
    memory_reset_peak_usage();
    $value = fastjson_decode($json);
    $delta = memory_get_peak_usage() - $before;
    unset($value);
    return max(0, $delta);
}

$before = memory_get_usage();
memory_reset_peak_usage();
$blob = str_repeat('a', 200000);
$tracks = memory_get_peak_usage() > $before + 10000;
unset($blob);

if ($tracks) {
    var_dump(peak($plain) + intdiv(strlen($plain), 2) < peak($escaped));
} else {
    echo "skip peak\n";
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
