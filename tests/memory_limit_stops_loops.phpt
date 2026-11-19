--TEST--
Decode, encode, and pointer-set stop at memory_limit instead of running to the hard limit
--EXTENSIONS--
fastjson
--SKIPIF--
<?php
if (!is_readable('/proc/self/maps')) print 'skip requires Linux procfs';
if (!function_exists('proc_open')) print 'skip proc_open unavailable';
?>
--FILE--
<?php
/* Engines that queue memory-limit errors (ZEND_HAS_OOM: PHP 8.6, and 8.4 and
 * 8.5 patch releases) let the allocation that crosses memory_limit succeed
 * and raise the error when the internal call returns. A loop that keeps
 * allocating reaches the hard limit (twice memory_limit by default), where
 * the engine prints "Hard memory limit" and calls _exit(137): no shutdown
 * functions, and under FrankenPHP the whole server process dies. Each case
 * must instead end in the ordinary memory-limit fatal error. Engines that
 * bail out at the limit produce the same output. */
$module = null;
foreach (file('/proc/self/maps', FILE_IGNORE_NEW_LINES) as $mapping) {
    if (preg_match('~\s(/\S+/fastjson\.so)$~', $mapping, $match)) {
        $module = $match[1];
        break;
    }
}
if ($module === null) {
    echo "FAIL: located module\n";
    exit;
}

function run_child(string $module, string $code): array {
    $code = 'register_shutdown_function(static function () { echo "shutdown ran\n"; });'
        . $code;
    $command = [
        '/usr/bin/env', 'USE_ZEND_ALLOC=1', 'USE_TRACKED_ALLOC=0',
        PHP_BINARY, '-n', '-d', "extension={$module}",
        '-d', 'memory_limit=64M', '-r', $code,
    ];
    $process = proc_open($command, [1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    $output = stream_get_contents($pipes[1]) . stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    return [proc_close($process), $output];
}

$cases = [
    /* yyjson's value pool doubles past the hard limit in one request. */
    'decode parse' => '$j = "[" . str_repeat("{},", 6000000) . "{}]"; fastjson_decode($j);',
    /* The parse fits; building 1.2M stdClass objects does not. */
    'decode walk' => '$j = "[" . str_repeat("{},", 1200000) . "{}]"; fastjson_decode($j);',
    'decode walk, assoc' => '$j = "[" . str_repeat("[0],", 900000) . "[0]]"; fastjson_decode($j, true);',
    /* SUBSTITUTE reserves three bytes per input byte, then copies the result. */
    'decode substitute' => '$j = "\"" . str_repeat("\\xff", 18000000) . "\""; fastjson_decode($j, false, 512, JSON_INVALID_UTF8_SUBSTITUTE);',
    'merge patch' => '$j = "[" . str_repeat("{},", 1200000) . "{}]"; fastjson_merge_patch("{}", "{\"a\":$j}");',
    /* 200 references to one 1 MiB string encode to 200 MiB. */
    'encode' => '$s = str_repeat("a", 1 << 20); fastjson_encode(array_fill(0, 200, $s));',
    'encode, throw' => 'try { fastjson_encode(array_fill(0, 200, str_repeat("a", 1 << 20)), JSON_THROW_ON_ERROR); } catch (Throwable $e) { echo "caught\n"; }',
    /* Pretty-printing 200k values nested 300 deep writes ~1.2 KB of indent each. */
    'pointer set' => '$j = "[" . str_repeat("[", 300) . "0" . str_repeat(",1", 200000) . str_repeat("]", 300) . ",0]"; fastjson_pointer_set($j, "/1", 2, 512, JSON_PRETTY_PRINT);',
];
foreach ($cases as $name => $code) {
    [$status, $output] = run_child($module, $code);
    $ok = $status === 255
        && str_contains($output, 'Allowed memory size of 67108864 bytes exhausted')
        && str_contains($output, 'shutdown ran')
        && !str_contains($output, 'Hard memory limit')
        && !str_contains($output, 'caught');
    echo $ok ? 'OK' : "FAIL (exit $status)", ": $name\n";
    if (!$ok) {
        echo substr($output, -400), "\n";
    }
}
?>
--EXPECT--
OK: decode parse
OK: decode walk
OK: decode walk, assoc
OK: decode substitute
OK: merge patch
OK: encode
OK: encode, throw
OK: pointer set
