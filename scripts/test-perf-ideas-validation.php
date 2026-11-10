<?php
/* Exercise the real case definitions and preflight without loading fastjson.
 * API doubles return each case's oracle; native runs verify the real APIs. */
namespace PerfIdeasValidationTest;
use \JsonSerializable;
use \RuntimeException;
use \Throwable;

function extension_loaded(string $name): bool { return true; }
function hrtime(bool $number = false): never { throw new RuntimeException('unexpected timing'); }
function memory_reset_peak_usage(): never { throw new RuntimeException('unexpected memory measurement'); }
function result(): mixed
{
    $index = $GLOBALS['probe_calls']++;
    $case = $GLOBALS['cases'][$index];
    if ($index === ($GLOBALS['reject_index'] ?? -1)) {
        return ($GLOBALS['reject_shape'] ?? false)
            ? \json_encode($case['expected'], JSON_FORCE_OBJECT) : null;
    }
    return ($case['json'] ?? false) ? \json_encode($case['expected']) : $case['expected'];
}
function fastjson_pointer_set(...$args): mixed { return result(); }
function fastjson_encode(...$args): mixed { return result(); }
function fastjson_decode(...$args): mixed { return result(); }
function fastjson_file_decode(...$args): mixed { return result(); }
function fastjson_merge_patch(...$args): mixed { return result(); }
function fastjson_validate(...$args): mixed { return result(); }

$source = file_get_contents(dirname(__DIR__) . '/bench/perf_ideas.php');
$source = substr($source, 5); // Drop only the opening PHP tag.
$preflight = strpos($source, "try {\n    validate_bench_cases(\$cases);");
$report = strpos($source, '$cpu = trim');
if ($preflight === false || $report === false || $preflight >= $report) {
    throw new RuntimeException('missing pre-report validation');
}
$imports = 'namespace ' . __NAMESPACE__ . '; use \\JsonSerializable; use \\RuntimeException; use \\Throwable;';
$GLOBALS['probe_calls'] = 0;
if (($argv[1] ?? '') === '--integration') {
    $GLOBALS['reject_index'] = (int)$argv[2];
    $GLOBALS['reject_shape'] = ($argv[3] ?? '') === 'shape';
    $argv = [__FILE__, '3'];
    eval($imports . substr($source, 0, $report));
    foreach ($cases as $validatedCase) {
        if (array_key_exists('expected', $validatedCase) || array_key_exists('json', $validatedCase)) {
            throw new RuntimeException('oracle retained in measured cases');
        }
    }
    if (isset($case) || isset($stringLeaf) || isset($intLeaf) || isset($arrayLeaf) || isset($merged)) {
        throw new RuntimeException('oracle-only copies retained');
    }
    echo "preflight passed\n";
    exit;
}
$argv = [__FILE__, '3'];
eval($imports . substr($source, 0, $preflight));
$count = count($cases);
validate_bench_cases($cases);
if ($GLOBALS['probe_calls'] !== $count) throw new RuntimeException('case not probed exactly once');

$rejections = 0;
$shapeRejections = 0;
foreach ($cases as $index => $case) {
    // Invalid types, malformed JSON, and valid-but-wrong results must fail.
    $wrong = ($case['json'] ?? false) ? ['null', '{}', '{bad', false, null] : [null, 'wrong', false, true, 0, 1];
    if (is_array($case['expected']) && !($case['json'] ?? false)) {
        $changed = $case['expected'];
        $changed['unexpected-key'] = true;
        $wrong[] = $changed;
    }
    if ($case['json'] ?? false) {
        $correctJson = \json_encode($case['expected'], JSON_THROW_ON_ERROR);
        $objectJson = \json_encode($case['expected'], JSON_FORCE_OBJECT | JSON_THROW_ON_ERROR);
        if ($objectJson !== $correctJson) {
            $wrong[] = $objectJson;
            if (startup($index, 'shape') !== [1, '', "cannot benchmark {$case['name']}: unexpected result\n"]) {
                throw new RuntimeException("accepted object-shaped arrays at startup for {$case['name']}");
            }
            $shapeRejections++;
        }
        // Equivalent whitespace and escape spellings remain valid.
        $good = $case;
        $prettyJson = \json_encode($case['expected'], JSON_PRETTY_PRINT | JSON_UNESCAPED_SLASHES | JSON_THROW_ON_ERROR);
        $good['fn'] = static fn() => $prettyJson;
        validate_bench_cases([$good]);
        if (str_contains($correctJson, '"tags":["red","green","blue"]')) {
            // Narrow regression: only the replacement leaf changes shape.
            $wrong[] = str_replace('"tags":["red","green","blue"]',
                '"tags":{"0":"red","1":"green","2":"blue"}', $correctJson);
        }
    }
    foreach ($wrong as $value) {
        if ($value === $case['expected']) continue;
        $bad = $case;
        $bad['fn'] = static fn() => $value;
        try {
            validate_bench_cases([$bad]);
            throw new \LogicException("accepted invalid result for {$case['name']}");
        } catch (RuntimeException $error) {
            if (!str_starts_with($error->getMessage(), "cannot benchmark {$case['name']}: ")) throw $error;
        }
        $rejections++;
    }
    $bad = $case;
    $bad['fn'] = static function (): never { throw new \ValueError('callback failed'); };
    try {
        validate_bench_cases([$bad]);
        throw new \LogicException('ignored callback exception');
    } catch (RuntimeException $error) {
        if ($error->getMessage() !== "cannot benchmark {$case['name']}: callback failed"
            || !$error->getPrevious() instanceof \ValueError) throw $error;
    }

    // Execute the actual startup/preflight, including the last case. No header
    // or timing may precede a rejection, and failures must exit nonzero.
    [$status, $out, $err] = startup($index);
    $reason = ($case['json'] ?? false) ? 'expected JSON string' : 'unexpected result';
    if ($status !== 1 || $out !== '' || $err !== "cannot benchmark {$case['name']}: $reason\n") {
        throw new RuntimeException("integration failed for {$case['name']}: $status/$out/$err");
    }
}
echo "$count valid fixtures, $rejections wrong results, $count exceptions and $count startup rejections passed\n";
echo "$shapeRejections object-shaped array startup rejections passed\n";

// Synthetic neighbours exercise container distinctions and strict scalar types
// independently of the current fixed dataset, including empty containers.
$neighbours = [
    [[], '[]', ['{}']],
    [(object)[], '{}', ['[]']],
    [[1, true, null], '[1,true,null]', ['{"0":1,"1":true,"2":null}', '[1.0,true,null]', '[1,1,null]']],
    [['items' => []], '{"items":[]}', ['{"items":{}}']],
    [['items' => (object)[]], '{"items":{}}', ['{"items":[]}']],
    [['value' => 1], '{"value":1}', ['{"value":1.0}', '{"value":"1"}', '{"value":true}']],
    [42, '42', ['42.0', '"42"', 'true']],
];
$neighbourRejections = 0;
foreach ($neighbours as [$expected, $valid, $invalid]) {
    $fixture = ['name' => 'shape neighbour', 'expected' => $expected, 'json' => true,
        'fn' => static fn() => $valid];
    validate_bench_cases([$fixture]);
    foreach ($invalid as $value) {
        $fixture['fn'] = static fn() => $value;
        try {
            validate_bench_cases([$fixture]);
            throw new \LogicException("accepted shape/type neighbour: $value");
        } catch (RuntimeException $error) {
            if ($error->getMessage() !== 'cannot benchmark shape neighbour: unexpected result') throw $error;
        }
        $neighbourRejections++;
    }
}
echo count($neighbours) . " valid neighbours, $neighbourRejections shape/type rejections passed\n";

function startup(int $index, string $mode = ''): array
{
    $process = proc_open([PHP_BINARY, '-n', __FILE__, '--integration', (string)$index, $mode],
        [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    if (!is_resource($process)) throw new RuntimeException('cannot start integration');
    fclose($pipes[0]);
    $out = stream_get_contents($pipes[1]);
    $err = stream_get_contents($pipes[2]);
    fclose($pipes[1]); fclose($pipes[2]);
    $status = proc_close($process);
    return [$status, $out, $err];
}
if (startup(-1) !== [0, "preflight passed\n", '']) {
    throw new RuntimeException('successful startup or oracle cleanup failed');
}
echo "successful startup and oracle cleanup passed\n";
