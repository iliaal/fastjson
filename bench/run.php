<?php
/*
 * Benchmark harness: fastjson vs ext/json (and optionally simdjson)
 * on canonical JSON corpora.
 *
 * Usage:
 *   php -d extension=$(pwd)/modules/fastjson.so \
 *       -d extension=/path/to/simdjson.so \
 *       bench/run.php [data-dir] [iterations]
 *
 * If simdjson is loaded, decode + validate rows gain a third column.
 * Encode is fastjson-vs-ext/json only (simdjson is decode-only).
 *
 * The "ext/json" column reflects whatever PHP build is hosting this
 * process. Run against ~/php-install-PHP-8.4-pr120 to compare against
 * the php-src#17734 SIMD-encoded ext/json; against an unpatched build
 * to compare against today's ext/json.
 *
 * Methodology:
 *   - Throughput: N iterations of encode + decode + validate per
 *     extension, hrtime(true) timing, slowest 10% dropped, median
 *     reported as MB/s using the source byte size as denominator.
 *   - The harness walks <data-dir>/*.json (large corpus) and
 *     <data-dir>/small/*.json (small corpus, where per-call overhead
 *     dominates and ns/call is the more useful number).
 *   - Encode is timed independently of decode: each file is decoded
 *     once up front, then encode is timed on the resulting PHP value.
 *   - Memory: a single fresh call per (file, op, ext) pair, with
 *     memory_reset_peak_usage() before. Reports peak heap (bytes)
 *     held during the call.
 *
 * Output: markdown to stdout.
 */

if (!extension_loaded('fastjson')) {
    fwrite(STDERR, "fastjson extension not loaded; pass -d extension=...\n");
    exit(1);
}
if (!extension_loaded('json')) {
    fwrite(STDERR, "ext/json not loaded; cannot compare\n");
    exit(1);
}
$haveSimdjson = extension_loaded('simdjson');

$dataDir = $argv[1] ?? __DIR__ . '/data';
$iterations = (int)($argv[2] ?? 50);

if (!is_dir($dataDir)) {
    fwrite(STDERR, "data dir not found: $dataDir\n  run bench/fetch-data.sh first\n");
    exit(1);
}

$largeFiles = glob($dataDir . '/*.json') ?: [];
$smallFiles = glob($dataDir . '/small/*.json') ?: [];
sort($largeFiles);
sort($smallFiles);

if (!$largeFiles && !$smallFiles) {
    fwrite(STDERR, "no .json files in $dataDir\n");
    exit(1);
}

/** Returns [median_ns, mean_ns] over $iterations runs, slowest 10% dropped. */
function bench_time(callable $callable, mixed $input, int $iterations): array
{
    $times = [];
    for ($i = 0; $i < $iterations; $i++) {
        $t0 = hrtime(true);
        $callable($input);
        $times[] = hrtime(true) - $t0;
    }
    sort($times);
    $drop = (int)floor($iterations * 0.1);
    $kept = array_slice($times, 0, $iterations - $drop);
    $median = $kept[(int)floor(count($kept) / 2)];
    $mean = array_sum($kept) / count($kept);
    return [$median, (int)$mean];
}

/** Single-call peak heap delta in bytes.
 *
 * Uses memory_get_*_usage() WITHOUT the `true` argument so we measure
 * actual byte-level emalloc accounting, not the 2MB chunks Zend
 * requests from the OS. The `true` argument rounds to chunk
 * granularity which makes most small ops appear as either 0 or 2MB. */
function bench_mem(callable $callable, mixed $input): int
{
    gc_collect_cycles();
    $baseline = memory_get_usage();
    memory_reset_peak_usage();
    $result = $callable($input);
    $peak = memory_get_peak_usage();
    unset($result);
    return max(0, $peak - $baseline);
}

function fmt_mbps(int $bytes, int $ns): string
{
    if ($ns <= 0) return '∞';
    return number_format(($bytes / 1048576.0) / ($ns / 1e9), 1);
}

function fmt_ns(int $ns): string
{
    if ($ns < 1000) return $ns . ' ns';
    if ($ns < 1_000_000) return number_format($ns / 1000, 1) . ' µs';
    return number_format($ns / 1_000_000, 2) . ' ms';
}

function fmt_bytes(int $b): string
{
    if ($b < 1024) return $b . ' B';
    if ($b < 1048576) return number_format($b / 1024, 1) . ' KB';
    return number_format($b / 1048576, 2) . ' MB';
}

function fmt_speedup(int $fast_ns, int $ext_ns): string
{
    if ($fast_ns <= 0) return '∞';
    return number_format($ext_ns / $fast_ns, 2) . 'x';
}

/** Memory ratio: fastjson / ext-json. >1 means fastjson uses MORE. */
function fmt_mem_ratio(int $fast_b, int $ext_b): string
{
    if ($ext_b <= 0) {
        if ($fast_b <= 0) return '-';
        /* ext/json used 0 bytes (streaming validate); fastjson used some. */
        return '∞';
    }
    return number_format($fast_b / $ext_b, 2) . 'x';
}

$phpVersion = phpversion();
$fastVersion = phpversion('fastjson');
$jsonVersion = phpversion('json');
$simdjsonVersion = $haveSimdjson ? phpversion('simdjson') : null;
$cpu = trim(shell_exec("grep -m1 'model name' /proc/cpuinfo | cut -d: -f2") ?: '?');
$cpu = preg_replace('/\s+/', ' ', $cpu);

echo "# fastjson benchmark\n\n";
echo "- PHP $phpVersion\n";
echo "- fastjson $fastVersion (yyjson 0.12.0)\n";
echo "- ext/json $jsonVersion\n";
if ($haveSimdjson) {
    echo "- simdjson $simdjsonVersion (decode + validate only)\n";
}
echo "- $iterations iterations per case (slowest 10% dropped)\n";
echo "- CPU: $cpu\n\n";

/* op => [title, has_simdjson, has_encode_role]
 * - decode_obj/decode_arr: simdjson has simdjson_decode($json, $assoc)
 * - validate: simdjson has simdjson_is_valid
 * - encode: no simdjson equivalent
 */
$ops = [
    'decode_obj' => ['Decode (objects)', true],
    'decode_arr' => ['Decode (assoc arrays)', true],
    'encode'     => ['Encode', false],
    'validate'   => ['Validate', true],
];

/** Returns [callables_by_label, prepared_input].
 *  callables_by_label is an ordered map: 'fastjson' => fn, 'ext/json' => fn,
 *  optionally 'simdjson' => fn. Order is preserved for table column ordering. */
function callables_for(string $op, string $json, bool $haveSimdjson): array
{
    switch ($op) {
        case 'decode_obj': {
            $fns = [
                'fastjson' => fn($s) => fastjson_decode($s, false),
                'ext/json' => fn($s) => json_decode($s, false),
            ];
            if ($haveSimdjson) {
                $fns['simdjson'] = fn($s) => simdjson_decode($s, false);
            }
            return [$fns, $json];
        }
        case 'decode_arr': {
            $fns = [
                'fastjson' => fn($s) => fastjson_decode($s, true),
                'ext/json' => fn($s) => json_decode($s, true),
            ];
            if ($haveSimdjson) {
                $fns['simdjson'] = fn($s) => simdjson_decode($s, true);
            }
            return [$fns, $json];
        }
        case 'encode': {
            $value = json_decode($json, true);
            $fns = [
                'fastjson' => fn($v) => fastjson_encode($v),
                'ext/json' => fn($v) => json_encode($v),
            ];
            return [$fns, $value];
        }
        case 'validate': {
            $fns = [
                'fastjson' => fn($s) => fastjson_validate($s),
                'ext/json' => fn($s) => json_validate($s),
            ];
            if ($haveSimdjson) {
                $fns['simdjson'] = fn($s) => simdjson_is_valid($s);
            }
            return [$fns, $json];
        }
    }
    throw new RuntimeException("unknown op $op");
}

/** Throughput section: emit per-file table; return totals
 * [$op => [labelMap, bytes]] where labelMap is label => total_ns. */
function throughput_section(string $title, array $files, array $ops, int $iterations,
                            bool $small, bool $haveSimdjson): array
{
    $totals = [];
    if (!$files) return $totals;
    echo "## $title\n\n";

    foreach ($ops as $op => [$opTitle, $opHasSimdjson]) {
        $useSimdjson = $opHasSimdjson && $haveSimdjson;
        echo "### $opTitle\n\n";

        // Header row varies by simdjson presence and small/large.
        if ($useSimdjson) {
            if ($small) {
                printf("| %-22s | %7s | %12s | %12s | %12s | %9s | %9s | %9s | %7s |\n",
                    'File', 'Size', 'fastjson', 'ext/json', 'simdjson',
                    'fast/call', 'ext/call', 'sj/call', 'speedup');
                printf("|%s|%s|%s|%s|%s|%s|%s|%s|%s|\n",
                    str_repeat('-', 24), str_repeat('-', 9),
                    str_repeat('-', 14), str_repeat('-', 14), str_repeat('-', 14),
                    str_repeat('-', 11), str_repeat('-', 11), str_repeat('-', 11),
                    str_repeat('-', 9));
            } else {
                printf("| %-22s | %8s | %12s | %12s | %12s | %7s |\n",
                    'File', 'Size', 'fastjson', 'ext/json', 'simdjson', 'speedup');
                printf("|%s|%s|%s|%s|%s|%s|\n",
                    str_repeat('-', 24), str_repeat('-', 10),
                    str_repeat('-', 14), str_repeat('-', 14), str_repeat('-', 14),
                    str_repeat('-', 9));
            }
        } else {
            if ($small) {
                printf("| %-22s | %7s | %12s | %12s | %9s | %9s | %7s |\n",
                    'File', 'Size', 'fastjson', 'ext/json',
                    'fast/call', 'ext/call', 'speedup');
                printf("|%s|%s|%s|%s|%s|%s|%s|\n",
                    str_repeat('-', 24), str_repeat('-', 9),
                    str_repeat('-', 14), str_repeat('-', 14),
                    str_repeat('-', 11), str_repeat('-', 11), str_repeat('-', 9));
            } else {
                printf("| %-22s | %8s | %12s | %12s | %7s |\n",
                    'File', 'Size', 'fastjson', 'ext/json', 'speedup');
                printf("|%s|%s|%s|%s|%s|\n",
                    str_repeat('-', 24), str_repeat('-', 10),
                    str_repeat('-', 14), str_repeat('-', 14), str_repeat('-', 9));
            }
        }

        $totalsByLabel = ['fastjson' => 0, 'ext/json' => 0];
        if ($useSimdjson) $totalsByLabel['simdjson'] = 0;
        $totBytes = 0;

        foreach ($files as $path) {
            $name = basename($path);
            $json = file_get_contents($path);
            $bytes = strlen($json);
            [$fns, $input] = callables_for($op, $json, $useSimdjson);

            $medianByLabel = [];
            foreach ($fns as $label => $fn) {
                [$med, ] = bench_time($fn, $input, $iterations);
                $medianByLabel[$label] = $med;
                $totalsByLabel[$label] += $med;
            }

            $fastMed = $medianByLabel['fastjson'];
            $extMed  = $medianByLabel['ext/json'];

            if ($useSimdjson) {
                $sjMed = $medianByLabel['simdjson'];
                if ($small) {
                    printf("| %-22s | %6s | %9s MB/s | %9s MB/s | %9s MB/s | %9s | %9s | %9s | %7s |\n",
                        $name, fmt_bytes($bytes),
                        fmt_mbps($bytes, $fastMed),
                        fmt_mbps($bytes, $extMed),
                        fmt_mbps($bytes, $sjMed),
                        fmt_ns($fastMed), fmt_ns($extMed), fmt_ns($sjMed),
                        fmt_speedup($fastMed, $extMed));
                } else {
                    printf("| %-22s | %7s | %9s MB/s | %9s MB/s | %9s MB/s | %7s |\n",
                        $name, fmt_bytes($bytes),
                        fmt_mbps($bytes, $fastMed),
                        fmt_mbps($bytes, $extMed),
                        fmt_mbps($bytes, $sjMed),
                        fmt_speedup($fastMed, $extMed));
                }
            } else {
                if ($small) {
                    printf("| %-22s | %6s | %9s MB/s | %9s MB/s | %9s | %9s | %7s |\n",
                        $name, fmt_bytes($bytes),
                        fmt_mbps($bytes, $fastMed),
                        fmt_mbps($bytes, $extMed),
                        fmt_ns($fastMed), fmt_ns($extMed),
                        fmt_speedup($fastMed, $extMed));
                } else {
                    printf("| %-22s | %7s | %9s MB/s | %9s MB/s | %7s |\n",
                        $name, fmt_bytes($bytes),
                        fmt_mbps($bytes, $fastMed),
                        fmt_mbps($bytes, $extMed),
                        fmt_speedup($fastMed, $extMed));
                }
            }
            $totBytes += $bytes;
        }
        $totals[$op] = [$totalsByLabel, $totBytes];
        echo "\n";
    }
    return $totals;
}

$largeTotals = throughput_section('Throughput, large corpus', $largeFiles, $ops, $iterations, false, $haveSimdjson);
$smallTotals = throughput_section('Throughput, small corpus', $smallFiles, $ops, $iterations, true, $haveSimdjson);

/* Memory section: all files combined, single-call peak heap delta. */
echo "## Memory peak (single-call delta)\n\n";
echo "Lower is better. The ratio is `fastjson / ext-json` peak heap; ";
echo "values **above 1.0 mean fastjson uses more memory**. This is the ";
echo "expected price of yyjson's two-stage model (build a doc, then walk ";
echo "into zvals or write a string), versus ext/json's streaming parser/";
echo "writer that emits results directly.\n\n";

$allFiles = [...$largeFiles, ...$smallFiles];
sort($allFiles);

$memTotals = [];
foreach ($ops as $op => [$opTitle, $opHasSimdjson]) {
    $useSimdjson = $opHasSimdjson && $haveSimdjson;
    echo "### $opTitle\n\n";
    if ($useSimdjson) {
        printf("| %-22s | %7s | %12s | %12s | %12s | %9s |\n",
            'File', 'Size', 'fastjson', 'ext/json', 'simdjson', 'fast/ext');
        printf("|%s|%s|%s|%s|%s|%s|\n",
            str_repeat('-', 24), str_repeat('-', 9),
            str_repeat('-', 14), str_repeat('-', 14), str_repeat('-', 14),
            str_repeat('-', 11));
    } else {
        printf("| %-22s | %7s | %12s | %12s | %9s |\n",
            'File', 'Size', 'fastjson', 'ext/json', 'fast/ext');
        printf("|%s|%s|%s|%s|%s|\n",
            str_repeat('-', 24), str_repeat('-', 9),
            str_repeat('-', 14), str_repeat('-', 14), str_repeat('-', 11));
    }

    $totalsByLabel = ['fastjson' => 0, 'ext/json' => 0];
    if ($useSimdjson) $totalsByLabel['simdjson'] = 0;

    foreach ($allFiles as $path) {
        $name = basename($path);
        $json = file_get_contents($path);
        $bytes = strlen($json);
        [$fns, $input] = callables_for($op, $json, $useSimdjson);

        $memByLabel = [];
        foreach ($fns as $label => $fn) {
            $memByLabel[$label] = bench_mem($fn, $input);
            $totalsByLabel[$label] += $memByLabel[$label];
        }

        if ($useSimdjson) {
            printf("| %-22s | %6s | %12s | %12s | %12s | %9s |\n",
                $name, fmt_bytes($bytes),
                fmt_bytes($memByLabel['fastjson']),
                fmt_bytes($memByLabel['ext/json']),
                fmt_bytes($memByLabel['simdjson']),
                fmt_mem_ratio($memByLabel['fastjson'], $memByLabel['ext/json']));
        } else {
            printf("| %-22s | %6s | %12s | %12s | %9s |\n",
                $name, fmt_bytes($bytes),
                fmt_bytes($memByLabel['fastjson']),
                fmt_bytes($memByLabel['ext/json']),
                fmt_mem_ratio($memByLabel['fastjson'], $memByLabel['ext/json']));
        }
    }
    $memTotals[$op] = $totalsByLabel;
    echo "\n";
}

echo "## Aggregate (sum across all files)\n\n";

function emit_aggregate_throughput(string $heading, array $totals, array $ops, bool $haveSimdjson): void
{
    echo "### $heading\n\n";
    if ($haveSimdjson) {
        printf("| %-22s | %8s | %12s | %12s | %12s | %7s |\n",
            'Operation', 'Bytes', 'fastjson', 'ext/json', 'simdjson', 'speedup');
        printf("|%s|%s|%s|%s|%s|%s|\n",
            str_repeat('-', 24), str_repeat('-', 10),
            str_repeat('-', 14), str_repeat('-', 14), str_repeat('-', 14),
            str_repeat('-', 9));
    } else {
        printf("| %-22s | %8s | %12s | %12s | %7s |\n",
            'Operation', 'Bytes', 'fastjson', 'ext/json', 'speedup');
        printf("|%s|%s|%s|%s|%s|\n",
            str_repeat('-', 24), str_repeat('-', 10),
            str_repeat('-', 14), str_repeat('-', 14), str_repeat('-', 9));
    }
    foreach ($ops as $op => [$opTitle, $opHasSimdjson]) {
        if (!isset($totals[$op])) continue;
        [$labelMap, $bytes] = $totals[$op];
        $f = $labelMap['fastjson'];
        $e = $labelMap['ext/json'];
        $haveSjForOp = $haveSimdjson && $opHasSimdjson && isset($labelMap['simdjson']);
        if ($haveSimdjson) {
            $sjCell = $haveSjForOp
                ? sprintf('%9s MB/s', fmt_mbps($bytes, $labelMap['simdjson']))
                : '            -';
            printf("| %-22s | %7s | %9s MB/s | %9s MB/s | %12s | %7s |\n",
                $opTitle, fmt_bytes($bytes),
                fmt_mbps($bytes, $f), fmt_mbps($bytes, $e),
                $sjCell,
                fmt_speedup($f, $e));
        } else {
            printf("| %-22s | %7s | %9s MB/s | %9s MB/s | %7s |\n",
                $opTitle, fmt_bytes($bytes),
                fmt_mbps($bytes, $f), fmt_mbps($bytes, $e),
                fmt_speedup($f, $e));
        }
    }
    echo "\n";
}

emit_aggregate_throughput('Throughput, large corpus', $largeTotals, $ops, $haveSimdjson);
if ($smallTotals) {
    emit_aggregate_throughput('Throughput, small corpus', $smallTotals, $ops, $haveSimdjson);
}

echo "### Memory peak (across all files)\n\n";
if ($haveSimdjson) {
    printf("| %-22s | %12s | %12s | %12s | %9s |\n",
        'Operation', 'fastjson', 'ext/json', 'simdjson', 'fast/ext');
    printf("|%s|%s|%s|%s|%s|\n",
        str_repeat('-', 24), str_repeat('-', 14),
        str_repeat('-', 14), str_repeat('-', 14), str_repeat('-', 11));
} else {
    printf("| %-22s | %12s | %12s | %9s |\n",
        'Operation', 'fastjson', 'ext/json', 'fast/ext');
    printf("|%s|%s|%s|%s|\n",
        str_repeat('-', 24), str_repeat('-', 14),
        str_repeat('-', 14), str_repeat('-', 11));
}
foreach ($ops as $op => [$opTitle, $opHasSimdjson]) {
    $labelMap = $memTotals[$op];
    $f = $labelMap['fastjson'];
    $e = $labelMap['ext/json'];
    $haveSjForOp = $haveSimdjson && $opHasSimdjson && isset($labelMap['simdjson']);
    if ($haveSimdjson) {
        $sjCell = $haveSjForOp ? fmt_bytes($labelMap['simdjson']) : '-';
        printf("| %-22s | %12s | %12s | %12s | %9s |\n",
            $opTitle, fmt_bytes($f), fmt_bytes($e), $sjCell,
            fmt_mem_ratio($f, $e));
    } else {
        printf("| %-22s | %12s | %12s | %9s |\n",
            $opTitle, fmt_bytes($f), fmt_bytes($e),
            fmt_mem_ratio($f, $e));
    }
}
echo "\n";

echo "_Throughput: higher is better; speedup = ext/json time / fastjson time. ";
echo "Memory: lower is better; ratio = fastjson peak / ext/json peak (>1 means fastjson uses more)._\n";
