<?php
/* Engines that queue memory-limit errors (ZEND_HAS_OOM: PHP 8.4.27+, 8.5.12+,
 * 8.6) let the allocation that crosses memory_limit succeed and raise the
 * error when the internal call returns. A loop that keeps allocating reaches
 * the hard limit (twice memory_limit by default), where the engine prints
 * "Hard memory limit" and calls _exit(137): no shutdown functions, and under
 * FrankenPHP the whole server process dies. Each case must instead end in the
 * ordinary memory-limit fatal error. Engines that bail out at the limit
 * produce the same output. */
function memory_limit_child(string $code): void
{
    $module = null;
    foreach (file('/proc/self/maps', FILE_IGNORE_NEW_LINES) as $mapping) {
        if (preg_match('~\s(/\S+/fastjson\.so)$~', $mapping, $match)) {
            $module = $match[1];
            break;
        }
    }
    if ($module === null) {
        echo "FAIL: located module\n";
        return;
    }
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
    $status = proc_close($process);
    $ok = $status === 255
        && str_contains($output, 'Allowed memory size of 67108864 bytes exhausted')
        && str_contains($output, 'shutdown ran')
        && !str_contains($output, 'Hard memory limit')
        && !str_contains($output, 'caught');
    echo $ok ? "OK\n" : "FAIL (exit $status)\n" . substr($output, -400) . "\n";
}
