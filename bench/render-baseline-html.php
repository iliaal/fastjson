<?php
/*
 * Render docs/baseline.html (the published benchmark page) from a
 * bench/run.php markdown baseline. Two-way: fastjson vs ext/json on the
 * PHP build that produced the markdown. Keeps the page regenerable so it
 * doesn't drift out of date.
 *
 *   php bench/render-baseline-html.php bench/baseline.md > docs/baseline.html
 */

$mdPath = $argv[1] ?? __DIR__ . '/baseline.md';
$md = @file_get_contents($mdPath);
if ($md === false) { fwrite(STDERR, "cannot read $mdPath\n"); exit(1); }
$lines = explode("\n", $md);

$meta = [];
$tables = [];               // "section||subsection" => [ [cells...], ... ]
$sec = $sub = '';
foreach ($lines as $l) {
    if (preg_match('/^- PHP (.+)$/', $l, $m))            $meta['php']  = 'PHP ' . $m[1];
    elseif (preg_match('/^- fastjson (\S+)(?: \(yyjson ([^)]+)\))?/', $l, $m)) {
        $meta['fast'] = $m[1];
        $meta['yyjson'] = $m[2] ?? '';
    }
    elseif (preg_match('/^- ext\/json (.+)$/', $l, $m))  $meta['ext']  = $m[1];
    elseif (preg_match('/^- (\d+) iterations/', $l, $m)) $meta['iter'] = $m[1];
    elseif (preg_match('/^- CPU: (.+)$/', $l, $m))       $meta['cpu']  = $m[1];

    if (strpos($l, '### ') === 0)      { $sub = trim(substr($l, 4)); continue; }
    if (strpos($l, '## ')  === 0)      { $sec = trim(substr($l, 3)); $sub = ''; continue; }
    if (($l[0] ?? '') !== '|')         continue;
    if (preg_match('/^\|[\s:\-|]+$/', $l)) continue;                 // separator row
    $cells = array_map('trim', explode('|', trim($l, '|')));
    $first = $cells[0] ?? '';
    if ($first === 'File' || $first === 'Operation') continue;       // header row
    $tables["$sec||$sub"][] = $cells;
}

function num($s) { return (float) str_replace(',', '', $s); }        // "1,065.0 MB/s" -> 1065.0
function mbps($s) { return number_format(round(num($s))); }          // -> "1,065"
function e($s) { return htmlspecialchars($s, ENT_QUOTES); }

/* Rows in "## Aggregate" live under its "### <subsection>" keys. */
function agg_find($tables, $subsection, $op) {
    foreach ($tables["Aggregate (sum across all files)||$subsection"] ?? [] as $r) {
        if (($r[0] ?? '') === $op) return $r;
    }
    return null;
}

/* Hero: two stacked bars (ext/json, fastjson) scaled to the larger. */
function hero($tables, $op, $captionExtra = '') {
    $r = agg_find($tables, 'Throughput, large corpus', $op) ?? [];
    // agg row: [Operation, Bytes, fastjson, ext/json, speedup]
    $fast = num($r[2] ?? '0'); $ext = num($r[3] ?? '0'); $sp = $r[4] ?? '';
    $max = max($fast, $ext, 1);
    $tick = (int) ceil($max / 500) * 500;
    ob_start(); ?>
<div class="hero">
  <div class="label-row">
    <div class="who">ext/json<span class="v"><?= e($GLOBALS['meta']['php']) ?></span></div>
    <div class="bar-wrap"><div class="bar" style="width: <?= round($ext / $max * 100, 1) ?>%; background: var(--c-ext);"></div></div>
    <div class="num"><?= mbps($r[3] ?? '0') ?> <span class="u">MB/s</span></div>
  </div>
  <div class="label-row">
    <div class="who">fastjson <?= e($GLOBALS['meta']['fast']) ?><span class="v"><?= e($GLOBALS['meta']['php']) ?></span></div>
    <div class="bar-wrap"><div class="bar" style="width: <?= round($fast / $max * 100, 1) ?>%; background: var(--c-fast);"></div></div>
    <div class="num"><?= mbps($r[2] ?? '0') ?> <span class="u">MB/s</span></div>
  </div>
  <div class="axis">
    <div></div>
    <div class="ticks"><span>0</span><span><?= number_format($tick / 2) ?></span><span><?= number_format($tick) ?> MB/s</span></div>
    <div></div>
  </div>
  <div class="caption">
    Aggregate on the 14.81&nbsp;MB large corpus. fastjson is <strong class="up" style="color:var(--c-up)"><?= e($sp) ?></strong> ext/json.<?= $captionExtra ? ' ' . $captionExtra : '' ?>
  </div>
</div>
<?php return ob_get_clean();
}

/* Large-corpus per-file table: File, Size, ext/json, fastjson, fastjson vs ext. */
function table_large($tables, $op) {
    $rows = $tables["Throughput, large corpus||$op"] ?? [];
    $agg  = agg_find($tables, 'Throughput, large corpus', $op) ?? [];
    ob_start(); ?>
<table class="matrix">
  <thead><tr>
    <th>File</th><th>Size</th>
    <th>ext/json<span class="sub">MB/s</span></th>
    <th>fastjson<span class="sub">MB/s</span></th>
    <th>fastjson vs<span class="sub">ext/json</span></th>
  </tr></thead>
  <tbody>
<?php foreach ($rows as $r):
    // [File, Size, fastjson, ext/json, speedup]
    $win = num($r[2]) >= num($r[3]); ?>
    <tr><td><?= e($r[0]) ?></td><td class="size"><?= e($r[1]) ?></td>
        <td><?= mbps($r[3]) ?></td>
        <td class="<?= $win ? 'win' : '' ?>"><?= mbps($r[2]) ?></td>
        <td class="<?= $win ? 'up' : 'down' ?>"><?= e($r[4]) ?></td></tr>
<?php endforeach; ?>
  </tbody>
  <tfoot>
    <tr><td>aggregate</td><td class="size"><?= e($agg[1] ?? '') ?></td>
        <td><?= mbps($agg[3] ?? '0') ?></td><td><?= mbps($agg[2] ?? '0') ?></td>
        <td class="up"><?= e($agg[4] ?? '') ?></td></tr>
  </tfoot>
</table>
<?php return ob_get_clean();
}

/* Small-corpus per-file table adds per-call ns. */
function table_small($tables, $op) {
    $rows = $tables["Throughput, small corpus||$op"] ?? [];
    $agg  = agg_find($tables, 'Throughput, small corpus', $op) ?? [];
    ob_start(); ?>
<table class="matrix">
  <thead><tr>
    <th>File</th><th>Size</th>
    <th>ext/json<span class="sub">MB/s</span></th>
    <th>fastjson<span class="sub">MB/s</span></th>
    <th>fast/call<span class="sub">ns</span></th>
    <th>fastjson vs<span class="sub">ext/json</span></th>
  </tr></thead>
  <tbody>
<?php foreach ($rows as $r):
    // [File, Size, fastjson, ext/json, fast/call, ext/call, speedup]
    $win = num($r[2]) >= num($r[3]); ?>
    <tr><td><?= e($r[0]) ?></td><td class="size"><?= e($r[1]) ?></td>
        <td><?= mbps($r[3]) ?></td>
        <td class="<?= $win ? 'win' : '' ?>"><?= mbps($r[2]) ?></td>
        <td><?= e($r[4]) ?></td>
        <td class="<?= $win ? 'up' : 'down' ?>"><?= e($r[6]) ?></td></tr>
<?php endforeach; ?>
  </tbody>
  <tfoot>
    <tr><td>aggregate</td><td class="size"><?= e($agg[1] ?? '') ?></td>
        <td><?= mbps($agg[3] ?? '0') ?></td><td><?= mbps($agg[2] ?? '0') ?></td>
        <td>&mdash;</td><td class="up"><?= e($agg[4] ?? '') ?></td></tr>
  </tfoot>
</table>
<?php return ob_get_clean();
}

/* Memory aggregate card (fastjson vs ext bars, normalized to the larger). */
function mem_card($tables, $op, $title) {
    $agg = agg_find($tables, 'Memory peak (across all files)', $op);
    if (!$agg) return '';
    // [Operation, fastjson, ext/json, ratio]
    $fast = num($agg[1]); $ext = num($agg[2]); $max = max($fast, $ext, 1);
    ob_start(); ?>
  <div class="mem-card">
    <h3><?= e($title) ?></h3>
    <div class="bar-row"><span class="who">ext/json</span><span class="b ext" style="width: <?= round($ext / $max * 120) ?>px;"></span><span class="v"><?= e($agg[2]) ?></span></div>
    <div class="bar-row"><span class="who">fastjson</span><span class="b" style="width: <?= round($fast / $max * 120) ?>px;"></span><span class="v"><?= e($agg[1]) ?></span></div>
    <div class="note">fastjson / ext-json peak: <strong><?= e($agg[3]) ?></strong></div>
  </div>
<?php return ob_get_clean();
}

$fastVer = $meta['fast'] ?? '';
echo '<!doctype html>' . "\n";
echo '<html lang="en">' . "\n<head>\n";
echo '<meta charset="utf-8">' . "\n";
echo '<meta name="viewport" content="width=device-width, initial-scale=1">' . "\n";
echo '<title>fastjson ' . e($fastVer) . ' baseline</title>' . "\n";
// CSS block (kept in sync with the page's visual style)
echo file_get_contents(__DIR__ . '/baseline.css');
echo "</head>\n<body>\n<div class=\"wrap\">\n\n";

// Aggregate speedups for the takeaway cards.
$aggLarge = [];
foreach (['Decode (objects)', 'Encode', 'Validate'] as $op) {
    $aggLarge[$op] = agg_find($tables, 'Throughput, large corpus', $op);
}
?>
<h1>fastjson <?= e($fastVer) ?> <span class="pill">benchmark baseline</span></h1>
<p class="lede">
A PHP extension that wraps <a href="https://github.com/ibireme/yyjson">yyjson<?= !empty($meta['yyjson']) ? ' ' . e($meta['yyjson']) : '' ?></a> behind a <code>fastjson_*</code> API mirroring ext/json. Side-by-side throughput, memory, and per-call latency against stock <code>ext/json</code> on a 21-file corpus, both hosted by <?= e($meta['php'] ?? '') ?>.
</p>

<p class="meta">
<span><strong>Hardware:</strong> <?= e($meta['cpu'] ?? '') ?></span>
<span><strong>Build:</strong> release, both PHP and extension <code>-O2</code></span>
<span><strong>Iterations:</strong> <?= e($meta['iter'] ?? '') ?> per case, slowest 10% dropped</span>
<span><strong>Corpus:</strong> 21 files &mdash; 15 large (14.81&nbsp;MB) + 6 small (64.6&nbsp;KB), from simdjson_php's <code>jsonexamples</code></span>
</p>

<div class="takeaway-grid">
<?php foreach ([['Decode (objects)', 'Decode', 'decode &rarr; <code>stdClass</code>'],
               ['Encode', 'Encode', 'encode a PHP value to JSON'],
               ['Validate', 'Validate', 'validate without building a tree']] as $c):
    $sp = $aggLarge[$c[0]][4] ?? ''; ?>
  <div class="takeaway">
    <div class="num"><?= e($sp) ?></div>
    <h3><?= $c[1] ?></h3>
    <p>vs ext/json, <?= $c[2] ?> (14.81&nbsp;MB large corpus aggregate).</p>
  </div>
<?php endforeach; ?>
</div>

<h2>Decode <span class="pill"><?= e($meta['php'] ?? '') ?></span></h2>
<p>Decode to <code>stdClass</code>. fastjson's decode path is unchanged from yyjson's reader; assoc-array decode tracks it closely (see <code>bench/baseline.md</code> for the full assoc table).</p>
<?= hero($tables, 'Decode (objects)') ?>
<h3>Per-file decode, large corpus</h3>
<?= table_large($tables, 'Decode (objects)') ?>
<div class="legend">
  <span><span class="sw" style="background: var(--c-fast);"></span>fastjson best</span>
  <span><span class="sw" style="background: var(--c-up);"></span>fastjson faster than ext/json</span>
</div>
<h3>Per-file decode, small corpus</h3>
<?= table_small($tables, 'Decode (objects)') ?>

<h2>Encode <span class="pill"><?= e($meta['php'] ?? '') ?></span></h2>
<p>fastjson 0.3.0+ encodes with a one-stage direct-write encoder (zval &rarr; <code>smart_str</code> via yyjson's scalar writers, no intermediate mutable document), which is where the largest wins on float- and pretty-heavy inputs come from.</p>
<?= hero($tables, 'Encode') ?>
<h3>Per-file encode, large corpus</h3>
<?= table_large($tables, 'Encode') ?>
<div class="legend">
  <span><span class="sw" style="background: var(--c-fast);"></span>fastjson best</span>
  <span><span class="sw" style="background: var(--c-down);"></span>ext/json faster</span>
</div>
<h3>Per-file encode, small corpus</h3>
<?= table_small($tables, 'Encode') ?>

<h2>Validate <span class="pill"><?= e($meta['php'] ?? '') ?></span></h2>
<p>fastjson's edge comes from vendor patch <code>P-002</code> (<code>YYJSON_READ_VALIDATE_ONLY</code>), a no-tree validate entry point that drops peak memory ~2.7&times; vs the stock read path. ext/json validates by fully decoding and discarding.</p>
<?= hero($tables, 'Validate') ?>
<h3>Per-file validate, large corpus</h3>
<?= table_large($tables, 'Validate') ?>
<div class="legend">
  <span><span class="sw" style="background: var(--c-fast);"></span>fastjson best</span>
  <span><span class="sw" style="background: var(--c-up);"></span>fastjson faster than ext/json</span>
</div>
<h3>Per-file validate, small corpus</h3>
<?= table_small($tables, 'Validate') ?>

<h2>Memory peak</h2>
<p>Single-call peak heap, <code>fastjson / ext-json</code>. Above 1.0 means fastjson uses more &mdash; the price of yyjson's build-a-doc-then-walk model on decode. Encode is at parity (direct-write, no mutable doc); validate trades memory for the no-tree fast path.</p>
<div class="mem-grid">
<?= mem_card($tables, 'Decode (objects)', 'Decode &rarr; stdClass') ?>
<?= mem_card($tables, 'Encode', 'Encode') ?>
<?= mem_card($tables, 'Validate', 'Validate') ?>
</div>

<p class="meta" style="border-top:1px solid var(--rule);padding-top:0.8rem;margin-top:2.5rem;">
Regenerate: <code>php -d extension=modules/fastjson.so bench/run.php bench/data 300 &gt; bench/baseline.md</code> then <code>php bench/render-baseline-html.php bench/baseline.md &gt; docs/baseline.html</code>. Source: <a href="https://github.com/iliaal/fastjson">github.com/iliaal/fastjson</a>.
</p>

</div>
</body>
</html>
