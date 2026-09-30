--TEST--
decode shares short repeated object keys
--EXTENSIONS--
fastjson
json
--SKIPIF--
<?php
if (getenv('USE_ZEND_ALLOC') === '0' && !getenv('USE_TRACKED_ALLOC')) {
    die('skip memory_get_usage() reads 0 without the Zend allocator');
}
?>
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
/* Padding past 4 KiB turns sharing on; the short form runs without it. */
$pad = str_repeat('0,', 2100);
foreach ($cases as $json) {
    same($json, false);
    same($json, true);
    same('[' . $pad . $json . ']', false);
    same('[' . $pad . $json . ']', true);
}

/* More distinct names than the table holds (misses after it fills),
 * names past the 64-byte limit, and a mostly unique document that turns
 * sharing off partway through. */
$wide = [];
for ($r = 0; $r < 20; $r++) {
    $row = [];
    for ($k = 0; $k < 400; $k++) {
        $row["field_$k"] = $k;
    }
    $row[str_repeat('L', 65) . $r % 3] = $r;
    $wide[] = $row;
}
$mostlyUnique = [];
for ($k = 0; $k < 5000; $k++) {
    $mostlyUnique["u$k"] = ['id' => $k];
}
foreach ([json_encode($wide), json_encode($mostlyUnique)] as $json) {
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
bool(true)
bool(true)
bool(true)
