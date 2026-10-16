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

foreach (['json_validate', 'memory_reset_peak_usage',
    'json_validate,memory_reset_peak_usage'] as $disabled) {
    foreach (['default' => null, 'minimum' => '1', 'zero' => '0'] as $label => $argument) {
        $cases["missing $disabled ($label)"] = [$argument, $argument === '0', $disabled];
    }
}

foreach ($cases as $name => $case) {
    [$argument, $invalid] = $case;
    $disabled = $case[2] ?? '';
    $command = [PHP_BINARY, '-n', '-d', "disable_functions=$disabled",
        $harness, __DIR__ . '/missing-corpus'];
    if ($argument !== null) {
        $command[] = $argument;
    }
    if ($invalid) {
        $expected = "iterations must be >= 1\n";
    } elseif ($disabled !== '' || PHP_VERSION_ID < 80300) {
        $expected = "benchmark requires PHP 8.3+ with json_validate() and memory_reset_peak_usage() available\n";
    } else {
        $expected = "fastjson extension not loaded; pass -d extension=...\n";
    }
    checkBenchmarkCommand($command, $expected, $name);
}

$perfHarness = dirname(__DIR__) . '/bench/perf_ideas.php';
foreach (['', 'json_validate', 'memory_reset_peak_usage',
    'json_validate,memory_reset_peak_usage'] as $disabled) {
    $expected = str_contains($disabled, 'memory_reset_peak_usage') || PHP_VERSION_ID < 80200
        ? "benchmark requires PHP 8.2+ with memory_reset_peak_usage() available\n"
        : "fastjson extension not loaded; pass -d extension=...\n";
    checkBenchmarkCommand(
        [PHP_BINARY, '-n', '-d', "disable_functions=$disabled", $perfHarness, '3'],
        $expected,
        "performance ideas, disabled=$disabled"
    );
}

function checkBenchmarkCommand(array $command, string $expected, string $name): void
{
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
    if ($status !== 1 || $stdout !== '' || $stderr !== $expected) {
        throw new RuntimeException(sprintf(
            "%s: status=%d stdout=%s stderr=%s",
            $name, $status, var_export($stdout, true), var_export($stderr, true)
        ));
    }
}

echo count($cases) + 4, " benchmark option and runtime prerequisite regressions passed\n";
