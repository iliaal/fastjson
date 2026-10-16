<?php
/* Exercise the real renderer; no extension or benchmark timing is needed. */
$root = dirname(__DIR__);
$baseline = file_get_contents($root . '/bench/baseline.md');
$fixture = tempnam(sys_get_temp_dir(), 'fastjson-render-');
if ($fixture === false || $baseline === false) {
    throw new RuntimeException('cannot prepare renderer fixture');
}
register_shutdown_function(static function () use ($fixture): void {
    unlink($fixture);
});

function heroScales(string $html): array
{
    preg_match_all('/<div class="hero">(.*?)<div class="caption">/s', $html, $heroes);
    if (count($heroes[1]) !== 3) {
        throw new RuntimeException('expected decode, encode, and validate heroes');
    }
    $scales = [];
    foreach ($heroes[1] as $hero) {
        preg_match_all('/<div class="bar" style="width: ([0-9.]+)%;/', $hero, $widths);
        preg_match('/<div class="ticks"><span>0<\/span><span>([0-9,]+)<\/span><span>([0-9,]+) MB\/s<\/span>/', $hero, $ticks);
        if (count($widths[1]) !== 2 || count($ticks) !== 3) {
            throw new RuntimeException('missing bar widths or axis ticks');
        }
        $scales[] = [
            (float)$widths[1][0], (float)$widths[1][1],
            (int)str_replace(',', '', $ticks[1]), (int)str_replace(',', '', $ticks[2]),
        ];
    }
    return $scales;
}

// fastjson, ext/json, expected ext width, fast width, midpoint, endpoint.
$cases = [
    [0, 0, 0.0, 0.0, 250, 500],
    [1, 0, 0.0, 0.2, 250, 500],
    [499, 200, 40.0, 99.8, 250, 500],
    [500, 200, 40.0, 100.0, 250, 500],
    [501, 200, 20.0, 50.1, 500, 1000],
    [999, 200, 20.0, 99.9, 500, 1000],
    [1000, 200, 20.0, 100.0, 500, 1000],
    [1001, 300, 20.0, 66.7, 750, 1500],
    [500, 500, 100.0, 100.0, 250, 500],
    [200, 501, 50.1, 20.0, 500, 1000],
    [748, 134.6, 13.5, 74.8, 500, 1000],
];
foreach ($cases as [$fast, $ext, $extWidth, $fastWidth, $middle, $end]) {
    $aggregate = "\n## Aggregate (sum across all files)\n\n### Throughput, large corpus\n";
    foreach (['Decode (objects)', 'Encode', 'Validate'] as $operation) {
        $aggregate .= "| $operation | 14.81 MB | $fast MB/s | $ext MB/s | fixture |\n";
    }
    // Replace the existing aggregate rows instead of adding duplicate entries.
    $input = preg_replace('/(### Throughput, large corpus\n).*?(?=\n###|\z)/s',
        '$1' . substr($aggregate, strpos($aggregate, '|')), $baseline);
    file_put_contents($fixture, $input);
    $process = proc_open([PHP_BINARY, '-n', $root . '/bench/render-baseline-html.php', $fixture],
        [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    if (!is_resource($process)) {
        throw new RuntimeException('cannot start renderer');
    }
    fclose($pipes[0]);
    $html = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    $status = proc_close($process);
    if ($status !== 0 || $stderr !== '') {
        throw new RuntimeException("renderer failed: $status $stderr");
    }
    foreach (heroScales($html) as $actual) {
        if ($actual !== [$extWidth, $fastWidth, $middle, $end]) {
            throw new RuntimeException("wrong scale for fast=$fast ext=$ext: " . json_encode($actual));
        }
    }
}

// This historical page predates the current Markdown baseline. Preserve its
// own measurements and axis ticks rather than regenerating it from newer data.
// Source: bench/baseline.md at 748f477a90694935285f6fd674c2b4000ec0da7b.
$historical = heroScales(file_get_contents($root . '/docs/baseline.html'));
if ($historical !== [
    [20.9, 54.2, 500, 1000],
    [16.6, 98.3, 500, 1000],
    [16.1, 84.0, 750, 1500],
]) {
    throw new RuntimeException('historical baseline bar scales changed');
}
echo "11 renderer cases (33 heroes) and historical scales passed\n";

function memoryCards(string $html): array
{
    preg_match_all('/  <div class="mem-card">.*?\n  <\/div>/s', $html, $matches);
    return $matches[0];
}

function expectedMemoryCard(string $title, string $fast, string $ext, string $ratio,
    int $fastWidth, int $extWidth): string
{
    return "  <div class=\"mem-card\">\n"
        . "    <h3>$title</h3>\n"
        . "    <div class=\"bar-row\"><span class=\"who\">ext/json</span><span class=\"b ext\" style=\"width: {$extWidth}px;\"></span><span class=\"v\">$ext</span></div>\n"
        . "    <div class=\"bar-row\"><span class=\"who\">fastjson</span><span class=\"b\" style=\"width: {$fastWidth}px;\"></span><span class=\"v\">$fast</span></div>\n"
        . "    <div class=\"note\">fastjson / ext-json peak: <strong>$ratio</strong></div>\n"
        . "  </div>";
}

// fastjson, ext/json, expected fastjson width, expected ext/json width.
$memoryCases = [
    ['14.91 MB', '150.6 KB', 120, 1],
    ['150.6 KB', '14.91 MB', 1, 120],
    ['1.00 KB', '512 B', 120, 60],
    ['512 B', '1.00 KB', 60, 120],
    ['1.00 MB', '512.0 KB', 120, 60],
    ['512.0 KB', '1.00 MB', 60, 120],
    ['1,024.00 MB', '512.00 MB', 120, 60],
    ['32 B', '64 B', 60, 120],
    ['0 B', '0 B', 0, 0],
    ['0 B', '1.00 KB', 0, 120],
    ['1.00 MB', '0 B', 120, 0],
];
foreach ($memoryCases as [$fast, $ext, $fastWidth, $extWidth]) {
    $rows = '';
    $expected = [];
    foreach (['Decode (objects)' => 'Decode &amp;rarr; stdClass', 'Encode' => 'Encode', 'Validate' => 'Validate'] as $op => $title) {
        $rows .= "| $op | $fast | $ext | fixture |\n";
        $expected[] = expectedMemoryCard($title, $fast, $ext, 'fixture', $fastWidth, $extWidth);
    }
    $input = preg_replace('/(### Memory peak \(across all files\)\n).*?(?=\n_|\z)/s', '$1' . $rows, $baseline);
    file_put_contents($fixture, $input);
    $process = proc_open([PHP_BINARY, '-n', $root . '/bench/render-baseline-html.php', $fixture],
        [0 => ['pipe', 'r'], 1 => ['pipe', 'w'], 2 => ['pipe', 'w']], $pipes);
    if (!is_resource($process)) throw new RuntimeException('cannot start memory renderer');
    fclose($pipes[0]);
    $html = stream_get_contents($pipes[1]);
    $stderr = stream_get_contents($pipes[2]);
    fclose($pipes[1]);
    fclose($pipes[2]);
    $status = proc_close($process);
    if ($status !== 0 || $stderr !== '' || memoryCards($html) !== $expected) {
        throw new RuntimeException("wrong memory cards for fast=$fast ext=$ext: $status $stderr");
    }
}

// Keep the published historical values; only the mixed-unit bar widths change.
if (memoryCards(file_get_contents($root . '/docs/baseline.html')) !== [
    expectedMemoryCard('Decode &amp;rarr; stdClass', '97.81 MB', '56.37 MB', '1.74x', 120, 69),
    expectedMemoryCard('Encode', '11.92 MB', '11.24 MB', '1.06x', 120, 113),
    expectedMemoryCard('Validate', '14.91 MB', '150.6 KB', '101.40x', 120, 1),
]) {
    throw new RuntimeException('historical memory card values or scales changed');
}
echo "11 memory cases (33 complete cards) and historical cards passed\n";
