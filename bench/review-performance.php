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

function reviewValidate(string $name, array $case): void
{
    try {
        $result = ($case['fn'])();
        $error = fastjson_last_error();
        if ($error !== ($case['error'] ?? JSON_ERROR_NONE)) {
            throw new RuntimeException("unexpected error $error");
        }
        $expected = $case['expected'];
        if ($case['json'] ?? false) {
            if (!is_string($result)) {
                throw new RuntimeException('expected JSON string');
            }
            $result = json_decode($result, false, 512, JSON_THROW_ON_ERROR);
            $expected = json_decode(json_encode($expected, JSON_THROW_ON_ERROR),
                false, 512, JSON_THROW_ON_ERROR);
        }
        // Preserve scalar types and JSON array/object shape, including nested
        // containers. Object identity and JSON escape spelling are irrelevant.
        $valid = is_array($expected) || is_object($expected)
            ? serialize($result) === serialize($expected)
            : $result === $expected;
        if (!$valid) {
            throw new RuntimeException('unexpected result');
        }
    } catch (Throwable $error) {
        throw new RuntimeException("cannot benchmark $name: " . $error->getMessage(), 0, $error);
    }
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
// Repeat whole three-byte groups: quote, backslash, newline.
$escaped128k = str_repeat("\"\\\n", intdiv(128 * 1024, 3));
$escaped256k = str_repeat("\"\\\n", intdiv(256 * 1024, 3));
$escaped512k = str_repeat("\"\\\n", intdiv(512 * 1024, 3));
$escaped1m = str_repeat("\"\\\n", intdiv(1024 * 1024, 3));
$lateQuote256k = str_repeat('a', 256 * 1024 - 1) . '"';
$lateQuote512k = str_repeat('a', 512 * 1024 - 1) . '"';
$unicode128k = str_repeat("é", intdiv(128 * 1024, 2));
$unicode256k = str_repeat("é", intdiv(256 * 1024, 2));
$unicode512k = str_repeat("é", intdiv(512 * 1024, 2));
$unicode1m = str_repeat("é", intdiv(1024 * 1024, 2));
$hex32k = str_repeat('<>&\'"', intdiv(32 * 1024, 5));
$cleanStringJson = '"' . $ascii1m . '"';
$invalidTailJson = '"' . $ascii1m . "\xFF\"";
$parseError = '{"payload":"' . str_repeat('a', 512 * 1024) . '",}';
$validateStrings1b = json_encode(array_fill(0, 100_000, 'a'));
$validateStrings8b = json_encode(array_fill(0, 50_000, 'abcdefgh'));

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

$mergedData = $mergeTargetData;
foreach ($mergePatchData as $key => $patch) {
    $mergedData[$key]['old'] = $patch['old'];
}
$pointerLeaf = json_decode($pointerBase, true);
$pointerLeaf['rows'][1000]['value'] = 'changed';
$pointerArray = json_decode($pointerBase, true);
$pointerArray['rows'][1000] = $pointerReplacement;

$cases = [
    'decode_packed_scalar_10k' => [
        'fn' => static fn() => fastjson_decode($packedScalar, true),
        'expected' => json_decode($packedScalar, true),
    ],
    'decode_packed_mixed_2k' => [
        'fn' => static fn() => fastjson_decode($packedMixed, true),
        'expected' => json_decode($packedMixed, true),
    ],
    'decode_object_5k' => [
        'fn' => static fn() => fastjson_decode($objectJson, false),
        'expected' => (object)$objectData,
    ],
    'decode_assoc_object_5k' => [
        'fn' => static fn() => fastjson_decode($objectJson, true),
        'expected' => $objectData,
    ],
    'merge_patch_2k' => [
        'fn' => static fn() => fastjson_merge_patch($mergeTarget, $mergePatch, true),
        'expected' => $mergedData,
    ],
    'file_decode_16m' => [
        'fn' => static fn() => fastjson_file_decode($file, true),
        'expected' => substr($fileJson, 1, -1),
    ],
    'file_get_plus_decode_16m' => [
        'fn' => static fn() => fastjson_decode(file_get_contents($file), true),
        'expected' => substr($fileJson, 1, -1),
    ],
    'pointer_set_leaf' => [
        'fn' => static fn() => fastjson_pointer_set($pointerBase, '/rows/1000/value', 'changed'),
        'expected' => $pointerLeaf, 'json' => true,
    ],
    'pointer_set_array_10k' => [
        'fn' => static fn() => fastjson_pointer_set($pointerBase, '/rows/1000', $pointerReplacement),
        'expected' => $pointerArray, 'json' => true,
    ],
    'pointer_set_root_array_10k' => [
        'fn' => static fn() => fastjson_pointer_set($pointerBase, '', $pointerReplacement),
        'expected' => $pointerReplacement, 'json' => true,
    ],
    'pointer_set_string_1m' => [
        'fn' => static fn() => fastjson_pointer_set('{}', '/value', $ascii1m),
        'expected' => ['value' => $ascii1m], 'json' => true,
    ],
    'decode_tolerant_clean_1m' => [
        'fn' => static fn() => fastjson_decode($cleanStringJson, true, 512, JSON_INVALID_UTF8_SUBSTITUTE),
        'expected' => $ascii1m,
    ],
    'decode_tolerant_invalid_tail_1m' => [
        'fn' => static fn() => fastjson_decode($invalidTailJson, true, 512, JSON_INVALID_UTF8_SUBSTITUTE),
        'expected' => $ascii1m . "\u{FFFD}",
    ],
    'decode_tolerant_strings_1b' => [
        'fn' => static fn() => fastjson_decode($validateStrings1b, true, 512, JSON_INVALID_UTF8_SUBSTITUTE),
        'expected' => array_fill(0, 100_000, 'a'),
    ],
    'decode_tolerant_strings_8b' => [
        'fn' => static fn() => fastjson_decode($validateStrings8b, true, 512, JSON_INVALID_UTF8_SUBSTITUTE),
        'expected' => array_fill(0, 50_000, 'abcdefgh'),
    ],
    'decode_error_tail_512k' => [
        'fn' => static fn() => fastjson_decode($parseError, true),
        'expected' => null, 'error' => JSON_ERROR_SYNTAX,
    ],
    'validate_strings_1b' => [
        'fn' => static fn() => fastjson_validate($validateStrings1b),
        'expected' => true,
    ],
    'validate_strings_8b' => [
        'fn' => static fn() => fastjson_validate($validateStrings8b),
        'expected' => true,
    ],
    'encode_ascii_64k' => [
        'fn' => static fn() => fastjson_encode($ascii64k),
        'expected' => $ascii64k, 'json' => true,
    ],
    'encode_ascii_128k' => [
        'fn' => static fn() => fastjson_encode($ascii128k),
        'expected' => $ascii128k, 'json' => true,
    ],
    'encode_ascii_below_256k' => [
        'fn' => static fn() => fastjson_encode($asciiBelow256k),
        'expected' => $asciiBelow256k, 'json' => true,
    ],
    'encode_ascii_256k' => [
        'fn' => static fn() => fastjson_encode($ascii256k),
        'expected' => $ascii256k, 'json' => true,
    ],
    'encode_ascii_512k' => [
        'fn' => static fn() => fastjson_encode($ascii512k),
        'expected' => $ascii512k, 'json' => true,
    ],
    'encode_ascii_below_1m' => [
        'fn' => static fn() => fastjson_encode($asciiBelow1m),
        'expected' => $asciiBelow1m, 'json' => true,
    ],
    'encode_ascii_1m' => [
        'fn' => static fn() => fastjson_encode($ascii1m),
        'expected' => $ascii1m, 'json' => true,
    ],
    'encode_escaped_128k' => [
        'fn' => static fn() => fastjson_encode($escaped128k),
        'expected' => $escaped128k, 'json' => true,
    ],
    'encode_escaped_256k' => [
        'fn' => static fn() => fastjson_encode($escaped256k),
        'expected' => $escaped256k, 'json' => true,
    ],
    'encode_escaped_512k' => [
        'fn' => static fn() => fastjson_encode($escaped512k),
        'expected' => $escaped512k, 'json' => true,
    ],
    'encode_escaped_1m' => [
        'fn' => static fn() => fastjson_encode($escaped1m),
        'expected' => $escaped1m, 'json' => true,
    ],
    'encode_late_quote_256k' => [
        'fn' => static fn() => fastjson_encode($lateQuote256k),
        'expected' => $lateQuote256k, 'json' => true,
    ],
    'encode_late_quote_512k' => [
        'fn' => static fn() => fastjson_encode($lateQuote512k),
        'expected' => $lateQuote512k, 'json' => true,
    ],
    'encode_unicode_128k' => [
        'fn' => static fn() => fastjson_encode($unicode128k),
        'expected' => $unicode128k, 'json' => true,
    ],
    'encode_unicode_256k' => [
        'fn' => static fn() => fastjson_encode($unicode256k),
        'expected' => $unicode256k, 'json' => true,
    ],
    'encode_unicode_512k' => [
        'fn' => static fn() => fastjson_encode($unicode512k),
        'expected' => $unicode512k, 'json' => true,
    ],
    'encode_unicode_1m' => [
        'fn' => static fn() => fastjson_encode($unicode1m),
        'expected' => $unicode1m, 'json' => true,
    ],
    'encode_hex_32k' => [
        'fn' => static fn() => fastjson_encode(
            $hex32k, JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT
        ),
        // HEX flags are an output-spelling contract, so compare exact bytes.
        'expected' => json_encode($hex32k,
            JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT),
    ],
    'pointer_set_hex_32k' => [
        'fn' => static fn() => fastjson_pointer_set(
            '{}', '/value', $hex32k, 512,
            JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT
        ),
        'expected' => json_encode(['value' => $hex32k],
            JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT),
    ],
    'encode_null' => [
        'fn' => static fn() => fastjson_encode(null),
        'expected' => null, 'json' => true,
    ],
    'encode_true' => [
        'fn' => static fn() => fastjson_encode(true),
        'expected' => true, 'json' => true,
    ],
    'encode_int' => [
        'fn' => static fn() => fastjson_encode(123456789),
        'expected' => 123456789, 'json' => true,
    ],
    'encode_double' => [
        'fn' => static fn() => fastjson_encode(12345.25),
        'expected' => 12345.25, 'json' => true,
    ],
];

$selectedCases = [];
if ($filter !== null && @preg_match($filter, '') === false) {
    fwrite(STDERR, "invalid benchmark filter: $filter\n");
    exit(1);
}
foreach ($cases as $name => $case) {
    if ($filter !== null && preg_match($filter, $name) !== 1) {
        continue;
    }
    $selectedCases[$name] = $case;
}

if ($selectedCases === []) {
    fwrite(STDERR, "no benchmark cases matched the filter\n");
    exit(1);
}

// Validate every selected fixture before collecting any timing samples.
try {
    foreach ($selectedCases as $name => $case) {
        reviewValidate($name, $case);
    }
} catch (RuntimeException $error) {
    fwrite(STDERR, $error->getMessage() . "\n");
    exit(1);
}
// Retain only the original callbacks during measurement, not the oracles.
foreach ($selectedCases as &$case) {
    $case = $case['fn'];
}
unset($case, $cases, $mergedData, $pointerLeaf, $pointerArray);

$results = [];
foreach ($selectedCases as $name => $fn) {
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
