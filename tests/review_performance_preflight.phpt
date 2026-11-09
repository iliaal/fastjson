--TEST--
Review benchmark rejects bad selected results before timing and releases oracles
--FILE--
<?php
$harness = dirname(__DIR__) . '/bench/review-performance.php';
$source = file_get_contents($harness);
$start = strpos($source, '$packedScalar =');
$end = strpos($source, '$selectedCases = [];');
if ($start === false || $end === false) throw new RuntimeException('missing fixtures');
eval(substr($source, $start, $end - $start));
$names = array_keys($cases);

$stub = <<<'STUB'
<?php
namespace ReviewPreflightDouble;
use \RuntimeException;
use \Throwable;
use \stdClass;
function extension_loaded(string $name): bool { return true; }
function hrtime(bool $number = false): never { throw new RuntimeException('unexpected timing'); }
function fastjson_last_error(): int { return $GLOBALS['error'] ?? 0; }
function result(): mixed
{
    $name = $GLOBALS['name'];
    $case = $GLOBALS['cases'][$name];
    $GLOBALS['calls'][] = $name;
    $GLOBALS['error'] = $case['error'] ?? 0;
    if ($name === $GLOBALS['reject']) return 'wrong';
    return ($case['json'] ?? false) ? json_encode($case['expected']) : $case['expected'];
}
function fastjson_encode(...$args): mixed { return result(); }
function fastjson_decode(...$args): mixed { return result(); }
function fastjson_file_decode(...$args): mixed { return result(); }
function fastjson_merge_patch(...$args): mixed { return result(); }
function fastjson_pointer_set(...$args): mixed { return result(); }
function fastjson_validate(...$args): mixed { return result(); }
$source = file_get_contents($argv[1]);
$GLOBALS['reject'] = $argv[2];
$filter = $argv[3];
$success = $argv[4] === 'success';
$GLOBALS['calls'] = [];
$argv = [__FILE__, '5', '20', $filter];
if ($success) $source = substr($source, 0, strpos($source, '$results = [];'));
eval('namespace ' . __NAMESPACE__ . '; use \\RuntimeException; use \\Throwable; use \\stdClass;' . substr($source, 5));
if (!$success) throw new RuntimeException('bad preflight completed');
foreach ($selectedCases as $callback) {
    if (!$callback instanceof \Closure) throw new RuntimeException('oracle retained in selected cases');
}
if (isset($cases) || isset($mergedData) || isset($pointerLeaf) || isset($pointerArray) || isset($case)) {
    throw new RuntimeException('oracle copies retained');
}
if ($GLOBALS['calls'] !== array_keys($selectedCases)) throw new RuntimeException('selection not probed exactly once');
echo 'preflight passed: ', count($selectedCases), "\n";
STUB;
$script = tempnam(sys_get_temp_dir(), 'fastjson-review-preflight-');
file_put_contents($script, $stub);
try {
    foreach ($names as $name) {
        // Select every case: a rejection in the final case must still precede
        // every timing probe, rather than merely skipping that case's timing.
        [$status, $out, $err] = run($script, $harness, $name, '//', 'failure');
        $reason = ($cases[$name]['json'] ?? false) ? 'Syntax error' : 'unexpected result';
        if ($status !== 1 || $out !== '' || $err !== "cannot benchmark $name: $reason\n") {
            throw new RuntimeException("bad startup for $name: $status/$out/$err");
        }
    }
    foreach (['//' => count($names), '/^encode_(null|true)$/' => 2, '/^decode_error_tail_512k$/' => 1] as $filter => $count) {
        if (run($script, $harness, 'not-selected', $filter, 'success') !== [0, "preflight passed: $count\n", '']) {
            throw new RuntimeException("failed successful preflight: $filter");
        }
    }
    // An invalid result belonging to an unselected case is never evaluated.
    if (run($script, $harness, 'encode_int', '/^encode_null$/', 'success') !== [0, "preflight passed: 1\n", '']) {
        throw new RuntimeException('unselected case was validated');
    }
    foreach (['[' => "invalid benchmark filter: [\n", '/^missing$/' => "no benchmark cases matched the filter\n"] as $filter => $message) {
        if (run($script, $harness, '', $filter, 'failure') !== [1, '', $message]) {
            throw new RuntimeException('filter behavior changed');
        }
    }
    echo count($names), " startup rejections, 4 successful selections and 2 filter rejections passed\n";
} finally {
    unlink($script);
}

function run(string ...$args): array
{
    $process = proc_open([PHP_BINARY, '-n', ...$args],
        [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    if (!is_resource($process)) throw new RuntimeException('cannot start fixture');
    fclose($pipes[0]);
    $out = stream_get_contents($pipes[1]);
    $err = stream_get_contents($pipes[2]);
    fclose($pipes[1]); fclose($pipes[2]);
    return [proc_close($process), $out, $err];
}
?>
--EXPECT--
41 startup rejections, 4 successful selections and 2 filter rejections passed
