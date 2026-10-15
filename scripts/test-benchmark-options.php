<?php
/* Exercise the actual CLI before it requires the extension or corpus. */
$harness = dirname(__DIR__) . '/bench/run.php';
$cases = [
    'zero' => ['0', true],
    'negative' => ['-1', true],
    'empty' => ['', true],
    'non-numeric' => ['invalid', true],
    'fraction-below-one' => ['0.5', true],
    'minimum' => ['1', false],
    'above-minimum' => ['2', false],
    'default' => [null, false],
    // Preserve the CLI's existing integer-cast behavior.
    'positive-fraction' => ['1.5', false],
    'numeric-prefix' => ['2suffix', false],
    'scientific-notation' => ['1e1', false],
];

foreach ($cases as $name => [$argument, $invalid]) {
    $command = [PHP_BINARY, '-n', $harness, __DIR__ . '/missing-corpus'];
    if ($argument !== null) {
        $command[] = $argument;
    }
    $process = proc_open($command, [
        0 => ['pipe', 'r'],
        1 => ['pipe', 'w'],
        2 => ['pipe', 'w'],
    ], $pipes);
    if (!is_resource($process)) {
        throw new RuntimeException("cannot start case $name");
    }
    fclose($pipes[0]);
    $stdout = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    $status = proc_close($process);
    $expected = $invalid
        ? "iterations must be >= 1\n"
        : "fastjson extension not loaded; pass -d extension=...\n";
    if ($status !== 1 || $stdout !== '' || $stderr !== $expected) {
        throw new RuntimeException(sprintf(
            "%s: status=%d stdout=%s stderr=%s",
            $name, $status, var_export($stdout, true), var_export($stderr, true)
        ));
    }
}

echo "benchmark option regressions passed\n";
