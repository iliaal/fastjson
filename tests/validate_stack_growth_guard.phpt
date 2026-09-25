--TEST--
Vendored yyjson validate stack growth: overflow guards hold (LP64)
--SKIPIF--
<?php
$cc = null;
foreach (['cc', 'gcc', 'clang'] as $candidate) {
    $path = trim((string) @shell_exec("command -v $candidate 2>/dev/null"));
    if ($path !== '') { $cc = $candidate; break; }
}
if ($cc === null) {
    die('skip no C compiler available');
}
?>
--FILE--
<?php
/* yyjson patch P-002's validate-only reader grows its container stack with
 * `stack_cap * 2` slots of sizeof(u64) bytes. Both conversions are checked
 * against USIZE_MAX before the allocator is called (patch P-006): without
 * them a capacity whose byte size wraps asks the allocator for a wrapped,
 * possibly zero-byte block, and the push then writes at the stale offset.
 *
 * This compiles the real `push_ctn` macro out of vendor/yyjson/yyjson.c
 * against a fake allocator and a static arena, so the boundary capacities
 * are exercised in a few kilobytes instead of gigabytes. The ILP32 build
 * lives in validate_stack_growth_ilp32.phpt; the public path through
 * fastjson_validate() in validate_stack_growth_depth.phpt. */

function fj_run(string $cmd, ?string &$out): int
{
    $lines = [];
    $code = 0;
    exec($cmd . ' 2>&1', $lines, $code);
    $out = implode("\n", $lines);
    return $code;
}

$cc = null;
foreach (['cc', 'gcc', 'clang'] as $candidate) {
    $path = trim((string) @shell_exec("command -v $candidate 2>/dev/null"));
    if ($path !== '') { $cc = $candidate; break; }
}

$root = dirname(__DIR__);
$harness = file_get_contents($root . '/tests/ilp32/validate_stack_growth.c');
$yyjson = file_get_contents($root . '/vendor/yyjson/yyjson.c');
if (!preg_match('/^#define push_ctn\(_is_obj\).*?^\} while \(false\)$/ms', $yyjson, $m)) {
    echo "push_ctn macro not found in vendor/yyjson/yyjson.c\n";
    exit(1);
}

$dir = sys_get_temp_dir() . '/fj_stack_growth_' . getmypid();
@mkdir($dir, 0777, true);
$gen = $dir . '/harness.c';
$bin = $dir . '/harness';
file_put_contents($gen, $m[0] . "\n\n" . $harness);

$build = escapeshellarg($cc) . ' -std=c99 -O1 -o ' . escapeshellarg($bin) . ' ' . escapeshellarg($gen);
$out = null;
$code = fj_run($build, $out);
if ($code !== 0) {
    echo "compile failed:\n$out\n";
    exit(1);
}

$code = fj_run(escapeshellarg($bin), $out);
echo 'guard probe: ', $code === 0 ? "PASS\n$out\n" : "FAIL\n$out\n";
@unlink($gen);
@unlink($bin);
exit($code === 0 ? 0 : 1);
?>
--EXPECTF--
guard probe: PASS
usize is %d bits
PASS: %d checks, 0 failures
