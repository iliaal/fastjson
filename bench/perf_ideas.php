<?php
/*
 * Focused benchmark for performance ideas that the broad corpus harness
 * does not cover: pointer_set, pretty-print, file_decode, validate, and
 * JsonSerializable. The output is intended for before/after runs on an
 * optimized PHP + optimized fastjson build.
 *
 * Usage:
 *   /path/to/release/php -d extension=$(pwd)/modules/fastjson.so \
 *       bench/perf_ideas.php [samples]
 */

if (!extension_loaded('fastjson')) {
    fwrite(STDERR, "fastjson extension not loaded; pass -d extension=...\n");
    exit(1);
}
if (!extension_loaded('json')) {
    fwrite(STDERR, "ext/json not loaded; cannot prepare benchmark inputs\n");
    exit(1);
}

$samples = (int)($argv[1] ?? 9);
if ($samples < 3) {
    fwrite(STDERR, "samples must be >= 3\n");
    exit(1);
}

ini_set('memory_limit', '-1');
gc_disable();

final class FastJsonBenchSerializable implements JsonSerializable
{
    public function __construct(private array $payload) {}

    public function jsonSerialize(): mixed
    {
        return $this->payload;
    }
}

function bench_dataset(int $rows): array
{
    $out = [
        'schema' => 1,
        'name' => 'fastjson-benchmark',
        'rows' => [],
        'summary' => [
            'enabled' => true,
            'ratio' => 17.25,
            'source' => '/var/tmp/fastjson/bench/input.json',
        ],
    ];

    for ($i = 0; $i < $rows; $i++) {
        $out['rows'][] = [
            'id' => $i,
            'name' => 'item-' . $i . '-' . str_repeat('x', 16),
            'active' => ($i & 1) === 0,
            'score' => $i + 0.125,
            'tags' => ['alpha', 'beta', 'gamma', 'delta'],
            'meta' => [
                'hash' => sha1((string)$i),
                'path' => '/api/v1/items/' . $i,
                'note' => str_repeat('note-' . ($i % 10), 4),
            ],
        ];
    }

    return $out;
}

function bench_deep_value(int $depth): array
{
    $value = ['leaf' => 'value'];
    for ($i = $depth; $i >= 1; $i--) {
        $value = [
            'level' => $i,
            'next' => $value,
            'label' => 'depth-' . $i,
        ];
    }
    return $value;
}

function fmt_ns(float $ns): string
{
    if ($ns < 1000) {
        return number_format($ns, 1) . ' ns';
    }
    if ($ns < 1000000) {
        return number_format($ns / 1000, 2) . ' us';
    }
    return number_format($ns / 1000000, 3) . ' ms';
}

function fmt_bytes(int $bytes): string
{
    if ($bytes < 1024) {
        return $bytes . ' B';
    }
    if ($bytes < 1048576) {
        return number_format($bytes / 1024, 1) . ' KB';
    }
    return number_format($bytes / 1048576, 2) . ' MB';
}

function median(array $values): float
{
    sort($values, SORT_NUMERIC);
    $count = count($values);
    $mid = intdiv($count, 2);
    if (($count & 1) === 1) {
        return (float)$values[$mid];
    }
    return ($values[$mid - 1] + $values[$mid]) / 2.0;
}

function bench_time(int $samples, int $loops, callable $fn): array
{
    for ($i = 0; $i < min(5, $loops); $i++) {
        $fn();
    }

    $sampleNs = [];
    for ($sample = 0; $sample < $samples; $sample++) {
        gc_collect_cycles();
        $t0 = hrtime(true);
        for ($i = 0; $i < $loops; $i++) {
            $fn();
        }
        $sampleNs[] = hrtime(true) - $t0;
    }

    $medianTotal = median($sampleNs);
    sort($sampleNs, SORT_NUMERIC);

    return [
        'median_ns' => $medianTotal / $loops,
        'best_ns' => $sampleNs[0] / $loops,
        'worst_ns' => $sampleNs[count($sampleNs) - 1] / $loops,
    ];
}

function bench_peak(callable $fn): int
{
    gc_collect_cycles();
    $baseline = memory_get_usage();
    memory_reset_peak_usage();
    $result = $fn();
    $peak = memory_get_peak_usage();
    unset($result);
    return max(0, $peak - $baseline);
}

$dataset = bench_dataset(900);
$json = json_encode($dataset, JSON_UNESCAPED_SLASHES);
if (!is_string($json)) {
    fwrite(STDERR, "failed to prepare JSON payload\n");
    exit(1);
}

$smallJson = '{"a":{"b":"old","c":1},"d":[1,2,3]}';
$invalidJson = substr($json, 0, -1) . ',}';
$prettyBroad = bench_dataset(650);
$prettyDeep = bench_deep_value(96);
$serializable = new FastJsonBenchSerializable(bench_dataset(120));

$tmpFile = tempnam(sys_get_temp_dir(), 'fastjson-bench-');
if ($tmpFile === false || file_put_contents($tmpFile, $json) === false) {
    fwrite(STDERR, "failed to prepare temp file\n");
    exit(1);
}
register_shutdown_function(static function () use ($tmpFile): void {
    if (is_file($tmpFile)) {
        unlink($tmpFile);
    }
});

$cases = [
    [
        'name' => 'pointer_set string leaf',
        'bytes' => strlen($json),
        'loops' => 120,
        'fn' => static fn() => fastjson_pointer_set($json, '/rows/450/name', 'renamed-item'),
    ],
    [
        'name' => 'pointer_set int leaf',
        'bytes' => strlen($json),
        'loops' => 120,
        'fn' => static fn() => fastjson_pointer_set($json, '/rows/450/id', 123456789),
    ],
    [
        'name' => 'pointer_set array leaf',
        'bytes' => strlen($json),
        'loops' => 100,
        'fn' => static fn() => fastjson_pointer_set($json, '/rows/450/tags', ['red', 'green', 'blue']),
    ],
    [
        'name' => 'pointer_set root scalar',
        'bytes' => 2,
        'loops' => 2000,
        'fn' => static fn() => fastjson_pointer_set('{}', '', 42),
    ],
    [
        'name' => 'pointer_set root replace large target',
        'bytes' => strlen($json),
        'loops' => 140,
        'fn' => static fn() => fastjson_pointer_set($json, '', 42),
    ],
    [
        'name' => 'pointer_set small string leaf',
        'bytes' => strlen($smallJson),
        'loops' => 2000,
        'fn' => static fn() => fastjson_pointer_set($smallJson, '/a/b', 'new'),
    ],
    [
        'name' => 'pretty encode broad',
        'bytes' => strlen($json),
        'loops' => 70,
        'fn' => static fn() => fastjson_encode($prettyBroad, JSON_PRETTY_PRINT),
    ],
    [
        'name' => 'pretty encode deep',
        'bytes' => 0,
        'loops' => 350,
        'fn' => static fn() => fastjson_encode($prettyDeep, JSON_PRETTY_PRINT),
    ],
    [
        'name' => 'decode assoc generated',
        'bytes' => strlen($json),
        'loops' => 180,
        'fn' => static fn() => fastjson_decode($json, true),
    ],
    [
        'name' => 'file_decode assoc generated',
        'bytes' => strlen($json),
        'loops' => 100,
        'fn' => static fn() => fastjson_file_decode($tmpFile, true),
    ],
    [
        'name' => 'merge_patch object merge',
        'bytes' => strlen($json),
        'loops' => 100,
        'fn' => static fn() => fastjson_merge_patch($json, '{"summary":{"enabled":false}}', true),
    ],
    [
        'name' => 'merge_patch scalar replace target',
        'bytes' => strlen($json),
        'loops' => 220,
        'fn' => static fn() => fastjson_merge_patch($json, '0', true),
    ],
    [
        'name' => 'validate valid generated',
        'bytes' => strlen($json),
        'loops' => 350,
        'fn' => static fn() => fastjson_validate($json),
    ],
    [
        'name' => 'validate invalid tail',
        'bytes' => strlen($invalidJson),
        'loops' => 350,
        'fn' => static fn() => fastjson_validate($invalidJson),
    ],
    [
        'name' => 'encode JsonSerializable',
        'bytes' => 0,
        'loops' => 600,
        'fn' => static fn() => fastjson_encode($serializable),
    ],
];

$cpu = trim((string)shell_exec("grep -m1 'model name' /proc/cpuinfo | cut -d: -f2"));
$cpu = preg_replace('/\s+/', ' ', $cpu ?: '?');

echo "# fastjson focused performance ideas\n\n";
echo "- PHP binary: " . PHP_BINARY . "\n";
echo "- PHP version: " . PHP_VERSION . "\n";
echo "- fastjson version: " . phpversion('fastjson') . "\n";
echo "- samples per case: " . $samples . "\n";
echo "- CPU: " . $cpu . "\n";
echo "- generated JSON payload: " . number_format(strlen($json)) . " bytes\n\n";

echo "| Case | Bytes | Loops/sample | Median/op | Best/op | Worst/op | Peak heap |\n";
echo "|------|------:|-------------:|----------:|--------:|---------:|----------:|\n";

foreach ($cases as $case) {
    $timing = bench_time($samples, $case['loops'], $case['fn']);
    $peak = bench_peak($case['fn']);
    printf(
        "| %s | %s | %d | %s | %s | %s | %s |\n",
        $case['name'],
        $case['bytes'] > 0 ? number_format($case['bytes']) : '-',
        $case['loops'],
        fmt_ns($timing['median_ns']),
        fmt_ns($timing['best_ns']),
        fmt_ns($timing['worst_ns']),
        fmt_bytes($peak)
    );
}
