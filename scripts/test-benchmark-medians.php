<?php
/* Load the actual timing and formatting functions without running a corpus.
 * A namespaced clock supplies deterministic samples, not wall-clock timings. */
namespace BenchmarkMedianTest;

$source = file_get_contents(dirname(__DIR__) . '/bench/run.php');
$wanted = ['bench_time', 'fmt_mbps', 'fmt_ns', 'fmt_speedup', 'fmt_bytes',
    'callables_for', 'throughput_section', 'emit_aggregate_throughput'];
$tokens = token_get_all($source);
$functions = [];
for ($i = 0; $i < count($tokens); $i++) {
    if (!is_array($tokens[$i]) || $tokens[$i][0] !== T_FUNCTION) continue;
    $name = $i + 1;
    while (is_array($tokens[$name]) && $tokens[$name][0] === T_WHITESPACE) $name++;
    if (!is_array($tokens[$name]) || !in_array($tokens[$name][1], $wanted, true)) continue;
    $code = '';
    $depth = 0;
    $opened = false;
    do {
        $token = $tokens[$i++];
        $text = is_array($token) ? $token[1] : $token;
        $code .= $text;
        if ($token === '{') { $depth++; $opened = true; }
        if ($token === '}') $depth--;
    } while (!$opened || $depth > 0);
    $i--;
    $functions[$tokens[$name][1]] = $code;
}
if (count($functions) !== count($wanted)) throw new \RuntimeException('missing benchmark functions');
eval('namespace ' . __NAMESPACE__ . ';' . implode("\n", $functions));

function hrtime(bool $asNumber): int
{
    if (!$asNumber || !$GLOBALS['clock']) throw new \RuntimeException('unexpected clock call');
    return array_shift($GLOBALS['clock']);
}
function expect($actual, $expected, string $label): void
{
    if ($actual !== $expected) throw new \RuntimeException($label . ': ' . var_export($actual, true));
}

// The shared preparation path protects both throughput and peak-memory calls.
// No clock samples may be consumed while probing the encoders.
function fastjson_encode($value) {
    return match ($GLOBALS['encode_probe_override'] ?? null) {
        'reject' => false,
        'accept' => 'null',
        default => \json_encode($value),
    };
}
$GLOBALS['clock'] = [];
$validEncodeInputs = ['null', 'false', '0', '""', '"false"', '[]', '{}',
    '{"nested":[1,true,null]}', '1e308', '-1e308', '1e-400'];
foreach ($validEncodeInputs as $json) {
    foreach ([false, true] as $optional) {
        [$encoders, $value] = callables_for('encode', $json, $optional);
        expect($value, \json_decode($json, true), 'prepared value preserved');
        expect(array_keys($encoders), ['fastjson', 'ext/json'], 'encode labels unchanged');
        foreach ($encoders as $encoder) {
            expect($encoder($value), \json_encode($value), 'valid encode preserved');
        }
        expect($GLOBALS['clock'], [], 'preparation is untimed');
    }
}
$invalidEncodeInputs = ['1e400', '-1e400', '[1e400]', '[-1e400]',
    '{"nested":[0,{"overflow":1e400}]}', '{"nested":[0,{"overflow":-1e400}]}'];
foreach ($invalidEncodeInputs as $json) {
    foreach ([null => 'fastjson', 'accept' => 'ext/json'] as $override => $label) {
        // Let the first encoder succeed to exercise rejection by the second.
        $GLOBALS['encode_probe_override'] = $override;
        try {
            callables_for('encode', $json, false);
            throw new \LogicException('accepted an unencodable value');
        } catch (\RuntimeException $error) {
            expect($error->getMessage(), "cannot benchmark encode: $label rejected the decoded corpus value",
                'failed encoder identified');
        }
        expect($GLOBALS['clock'], [], 'rejection precedes timing');
    }
}
$GLOBALS['encode_probe_override'] = 'reject';
try {
    callables_for('encode', 'null', false);
    throw new \LogicException('ignored fastjson failure for encodable input');
} catch (\RuntimeException $error) {
    expect($error->getMessage(), 'cannot benchmark encode: fastjson rejected the decoded corpus value',
        'fastjson failure independent of ext/json');
}
unset($GLOBALS['encode_probe_override']);
echo "encode preparation: 22 valid cases and 13 encoder rejection cases passed\n";

// Counts around 10% trim boundaries, including default 50 and documented 100.
$counts = [1, 2, 3, 4, 9, 10, 11, 12, 19, 20, 21, 22, 49, 50, 51, 99, 100, 101];
foreach ($counts as $count) {
    // Descending samples exercise sorting; consecutive samples also yield
    // half-nanosecond medians.
    foreach ([1, 2] as $step) {
        $samples = array_map(static fn($i) => 100 + $i * $step, range(0, $count - 1));
        $GLOBALS['clock'] = [];
        foreach (array_reverse($samples) as $sample) {
            $GLOBALS['clock'][] = 1000;
            $GLOBALS['clock'][] = 1000 + $sample;
        }
        $calls = 0;
        $median = bench_time(static function ($input) use (&$calls): void {
            expect($input, 'input', 'callback input');
            $calls++;
        }, 'input', $count);
        $kept = $count - (int)floor($count * 0.1);
        $expected = 100 + ($kept - 1) * $step / 2;
        expect((float)$median, (float)$expected, "median n=$count step=$step");
        expect($calls, $count, 'call count');
        expect($GLOBALS['clock'], [], 'clock fully consumed');
    }
}
expect(fmt_ns(100.5), '100.5 ns', 'fractional latency');
expect(fmt_ns(100), '100 ns', 'integral latency');
expect(fmt_ns(1000), '1.0 µs', 'microsecond boundary');
expect(fmt_ns(1000000), '1.00 ms', 'millisecond boundary');
expect(fmt_mbps(1048576, 1000000000.5), '1.0', 'fractional throughput');
expect(fmt_mbps(1, 0.5), '1,907.3', 'sub-nanosecond throughput');
expect(fmt_speedup(0.5, 1.5), '3.00x', 'fractional speedup');
expect(fmt_mbps(1, 0), '∞', 'zero throughput');
expect(fmt_speedup(0, 1), '∞', 'zero speedup');
// Exercise real per-file and aggregate consumers, including optional simdjson.
function fastjson_decode($json, $assoc) { return \json_decode($json, $assoc); }
function simdjson_decode($json, $assoc) { return \json_decode($json, $assoc); }
$fixture = tempnam(sys_get_temp_dir(), 'fastjson-median-');
if ($fixture === false) throw new \RuntimeException('cannot create fixture');
try {
    file_put_contents($fixture, '{"value":1}');
    $GLOBALS['clock'] = [];
    foreach ([1, 2] as $file) {
        foreach ([1, 2, 3] as $label) {
            foreach ([$label, $label + 1] as $sample) {
                $GLOBALS['clock'][] = 1000;
                $GLOBALS['clock'][] = 1000 + $sample;
            }
        }
    }
    $ops = ['decode_obj' => ['Decode (objects)', true]];
    ob_start();
    $totals = throughput_section('fixture', [$fixture, $fixture], $ops, 2, true, true);
    $rows = ob_get_clean();
    expect($totals['decode_obj'][0], ['fastjson' => 3.0, 'ext/json' => 5.0, 'simdjson' => 7.0], 'aggregate timings');
    expect(substr_count($rows, '1.5 ns'), 2, 'per-file fractional latency');
    expect(substr_count($rows, '1.67x'), 2, 'per-file fractional speedup');
    ob_start();
    emit_aggregate_throughput('fixture totals', $totals, $ops, true);
    $aggregate = ob_get_clean();
    expect(substr_count($aggregate, '1.67x'), 1, 'aggregate speedup');
    expect($GLOBALS['clock'], [], 'integration clock consumed');
} finally {
    unlink($fixture);
}
echo "36 deterministic timing cases, 9 formatting cases, and per-file/aggregate propagation passed\n";
