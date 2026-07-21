<?php

if (!extension_loaded('fastjson')) {
    fwrite(STDERR, "load fastjson with -d extension=/path/to/fastjson.so\n");
    exit(1);
}

$samples = max(5, (int)($argv[1] ?? 9));
$targetMs = max(20, (int)($argv[2] ?? 100));
$filter = $argv[3] ?? null;
ini_set('memory_limit', '-1');
gc_disable();

function reviewMedian(array $values): float
{
    sort($values, SORT_NUMERIC);
    $middle = intdiv(count($values), 2);
    return count($values) % 2
        ? (float)$values[$middle]
        : ($values[$middle - 1] + $values[$middle]) / 2;
}

function reviewMeasure(callable $fn, int $samples, int $targetMs): array
{
    $sink = $fn();
    $start = hrtime(true);
    for ($i = 0; $i < 3; $i++) {
        $sink = $fn();
    }
    $probe = max(1, hrtime(true) - $start);
    $perCall = $probe / 3;
    $loops = (int)max(1, min(2_000_000,
        ceil(($targetMs * 1_000_000) / $perCall)));

    $times = [];
    for ($sample = 0; $sample < $samples; $sample++) {
        $start = hrtime(true);
        for ($i = 0; $i < $loops; $i++) {
            $sink = $fn();
        }
        $times[] = (hrtime(true) - $start) / $loops;
    }
    unset($sink);

    return [
        'median_ns' => reviewMedian($times),
        'best_ns' => min($times),
        'worst_ns' => max($times),
        'loops' => $loops,
    ];
}

$packedScalar = json_encode(range(0, 9999));
$packedMixed = json_encode(array_map(
    static fn(int $i): array => [$i, "item-$i", ($i & 1) === 0, $i + 0.25],
    range(0, 1999)
));
$objectData = [];
for ($i = 0; $i < 5000; $i++) {
    $objectData["key-$i"] = $i;
}
$objectJson = json_encode($objectData);

$mergeTargetData = [];
$mergePatchData = [];
for ($i = 0; $i < 2000; $i++) {
    $mergeTargetData["key-$i"] = ['old' => $i, 'keep' => true];
    if (($i & 1) === 0) {
        $mergePatchData["key-$i"] = ['old' => -$i];
    }
}
$mergeTarget = json_encode($mergeTargetData);
$mergePatch = json_encode($mergePatchData);

$ascii64k = str_repeat('a', 64 * 1024);
$ascii128k = str_repeat('a', 128 * 1024);
$asciiBelow256k = str_repeat('a', 256 * 1024 - 1);
$ascii256k = str_repeat('a', 256 * 1024);
$ascii512k = str_repeat('a', 512 * 1024);
$asciiBelow1m = str_repeat('a', 1024 * 1024 - 1);
$ascii1m = str_repeat('a', 1024 * 1024);
$escaped128k = str_repeat('"\\\n', intdiv(128 * 1024, 3));
$escaped256k = str_repeat('"\\\n', intdiv(256 * 1024, 3));
$escaped512k = str_repeat('"\\\n', intdiv(512 * 1024, 3));
$escaped1m = str_repeat('"\\\n', intdiv(1024 * 1024, 3));
$unicode128k = str_repeat("é", intdiv(128 * 1024, 2));
$unicode256k = str_repeat("é", intdiv(256 * 1024, 2));
$unicode512k = str_repeat("é", intdiv(512 * 1024, 2));
$unicode1m = str_repeat("é", intdiv(1024 * 1024, 2));
$hex32k = str_repeat('<>&\'"', intdiv(32 * 1024, 5));
$cleanStringJson = '"' . $ascii1m . '"';
$invalidTailJson = '"' . $ascii1m . "\xFF\"";
$parseError = '{"payload":"' . str_repeat('a', 512 * 1024) . '",}';

$pointerBase = json_encode([
    'rows' => array_map(
        static fn(int $i): array => ['id' => $i, 'value' => "item-$i"],
        range(0, 1999)
    ),
    'meta' => ['version' => 1],
]);
$pointerReplacement = range(0, 9999);

$fileJson = '"' . str_repeat('f', 16 * 1024 * 1024) . '"';
$file = tempnam(sys_get_temp_dir(), 'fastjson-review-');
if ($file === false || file_put_contents($file, $fileJson) === false) {
    fwrite(STDERR, "failed to create benchmark file\n");
    exit(1);
}
register_shutdown_function(static function () use ($file): void {
    if (is_file($file)) {
        unlink($file);
    }
});

$cases = [
    'decode_packed_scalar_10k' => static fn() => fastjson_decode($packedScalar, true),
    'decode_packed_mixed_2k' => static fn() => fastjson_decode($packedMixed, true),
    'decode_object_5k' => static fn() => fastjson_decode($objectJson, false),
    'decode_assoc_object_5k' => static fn() => fastjson_decode($objectJson, true),
    'merge_patch_2k' => static fn() => fastjson_merge_patch($mergeTarget, $mergePatch, true),
    'file_decode_16m' => static fn() => fastjson_file_decode($file, true),
    'file_get_plus_decode_16m' => static fn() => fastjson_decode(file_get_contents($file), true),
    'pointer_set_leaf' => static fn() => fastjson_pointer_set($pointerBase, '/rows/1000/value', 'changed'),
    'pointer_set_array_10k' => static fn() => fastjson_pointer_set($pointerBase, '/rows/1000', $pointerReplacement),
    'pointer_set_root_array_10k' => static fn() => fastjson_pointer_set($pointerBase, '', $pointerReplacement),
    'pointer_set_string_1m' => static fn() => fastjson_pointer_set('{}', '/value', $ascii1m),
    'decode_tolerant_clean_1m' => static fn() => fastjson_decode($cleanStringJson, true, 512, JSON_INVALID_UTF8_SUBSTITUTE),
    'decode_tolerant_invalid_tail_1m' => static fn() => fastjson_decode($invalidTailJson, true, 512, JSON_INVALID_UTF8_SUBSTITUTE),
    'decode_error_tail_512k' => static fn() => fastjson_decode($parseError, true),
    'encode_ascii_64k' => static fn() => fastjson_encode($ascii64k),
    'encode_ascii_128k' => static fn() => fastjson_encode($ascii128k),
    'encode_ascii_below_256k' => static fn() => fastjson_encode($asciiBelow256k),
    'encode_ascii_256k' => static fn() => fastjson_encode($ascii256k),
    'encode_ascii_512k' => static fn() => fastjson_encode($ascii512k),
    'encode_ascii_below_1m' => static fn() => fastjson_encode($asciiBelow1m),
    'encode_ascii_1m' => static fn() => fastjson_encode($ascii1m),
    'encode_escaped_128k' => static fn() => fastjson_encode($escaped128k),
    'encode_escaped_256k' => static fn() => fastjson_encode($escaped256k),
    'encode_escaped_512k' => static fn() => fastjson_encode($escaped512k),
    'encode_escaped_1m' => static fn() => fastjson_encode($escaped1m),
    'encode_unicode_128k' => static fn() => fastjson_encode($unicode128k),
    'encode_unicode_256k' => static fn() => fastjson_encode($unicode256k),
    'encode_unicode_512k' => static fn() => fastjson_encode($unicode512k),
    'encode_unicode_1m' => static fn() => fastjson_encode($unicode1m),
    'encode_hex_32k' => static fn() => fastjson_encode(
        $hex32k,
        JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT
    ),
    'pointer_set_hex_32k' => static fn() => fastjson_pointer_set(
        '{}', '/value', $hex32k, 512,
        JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT
    ),
    'encode_null' => static fn() => fastjson_encode(null),
    'encode_true' => static fn() => fastjson_encode(true),
    'encode_int' => static fn() => fastjson_encode(123456789),
    'encode_double' => static fn() => fastjson_encode(12345.25),
];

$results = [];
foreach ($cases as $name => $fn) {
    if ($filter !== null && preg_match($filter, $name) !== 1) {
        continue;
    }
    $results[$name] = reviewMeasure($fn, $samples, $targetMs);
}

echo json_encode([
    'meta' => [
        'php' => PHP_VERSION,
        'fastjson' => phpversion('fastjson'),
        'samples' => $samples,
        'target_ms' => $targetMs,
        'timestamp_utc' => gmdate(DATE_ATOM),
    ],
    'cases' => $results,
], JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES), "\n";
