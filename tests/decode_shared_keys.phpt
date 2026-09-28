--TEST--
decode shares short repeated object keys
--EXTENSIONS--
fastjson
json
--INI--
memory_limit=-1
--FILE--
<?php

function same(string $json, bool $assoc): void
{
    $fast = fastjson_decode($json, $assoc);
    $ext = json_decode($json, $assoc);
    var_dump($fast == $ext);
}

$cases = [
    '{}',
    '[]',
    '{"0":1,"1":2,"00":3}',
    '[{"id":1,"name":"n"},{"id":2,"name":"n"}]',
    '{"a":1,"a":2}',
    '{"é":1}',
    '{"":1}',
];
foreach ($cases as $json) {
    same($json, false);
    same($json, true);
}

function live(string $json): int
{
    gc_collect_cycles();
    $before = memory_get_usage();
    $value = fastjson_decode($json);
    $delta = memory_get_usage() - $before;
    unset($value);
    return $delta;
}

$one = '{"id":1,"name":"abcdef","on":true,"n":1}';
$rep = '[' . implode(',', array_fill(0, 4000, $one)) . ']';
$uniq = [];
for ($i = 0; $i < 4000; $i++) {
    $uniq[] = '{"k' . $i . 'aa":1,"m' . $i . 'bb":"abcdef","q' . $i . 'cc":true,"r' . $i . 'dd":1}';
}
$uniqJson = '[' . implode(',', $uniq) . ']';

/* Same value shape. Sharing the four repeated names drops more than the
 * extra bytes of the unique-key document. Live usage, not peak: the ASAN
 * job sets USE_TRACKED_ALLOC, which does not advance memory_get_peak_usage(). */
var_dump(live($uniqJson) - live($rep) > 200000);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
