--TEST--
Vendored yyjson validate stack growth: overflow guards hold (ILP32)
--SKIPIF--
<?php
/* Needs a C compiler plus 32-bit headers: with -m32, sizeof(usize) is 4 and
 * USIZE_MAX is the 32-bit value the overflow arithmetic wraps against. */
$cc = null;
foreach (['cc', 'gcc', 'clang'] as $candidate) {
    $path = trim((string) @shell_exec("command -v $candidate 2>/dev/null"));
    if ($path !== '') { $cc = $candidate; break; }
}
if ($cc === null) {
    die('skip no C compiler available');
}
$dir = sys_get_temp_dir() . '/fj_m32_' . getmypid();
@mkdir($dir, 0777, true);
$src = $dir . '/probe.c';
$bin = $dir . '/probe';
file_put_contents($src, "int main(void) { return sizeof(void *) == 4 ? 0 : 1; }\n");
$code = 0;
$out = [];
exec(escapeshellarg($cc) . ' -m32 -o ' . escapeshellarg($bin) . ' '
    . escapeshellarg($src) . ' 2>&1', $out, $code);
if ($code === 0) {
    exec(escapeshellarg($bin) . ' 2>&1', $out, $code);
}
@unlink($src);
@unlink($bin);
@rmdir($dir);
if ($code !== 0) {
    die('skip no working -m32 multilib toolchain');
}
?>
--FILE--
<?php
/* See validate_stack_growth_guard.phpt. This is the same harness built for
 * 32-bit, where the finding's arithmetic is real: doubling a capacity of
 * 2^28 and scaling by sizeof(u64) wraps to a zero-byte allocation request.
 * That capacity needs a 256 MiB document to reach through
 * fastjson_validate(), so it is driven directly against the real macro with
 * a fake allocator and a static arena. */

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

$dir = sys_get_temp_dir() . '/fj_stack_growth_m32_' . getmypid();
@mkdir($dir, 0777, true);
$gen = $dir . '/harness.c';
$bin = $dir . '/harness';
file_put_contents($gen, $m[0] . "\n\n" . $harness);

$build = escapeshellarg($cc) . ' -m32 -std=c99 -O1 -o ' . escapeshellarg($bin) . ' ' . escapeshellarg($gen);
$out = null;
$code = fj_run($build, $out);
if ($code !== 0) {
    echo "compile failed:\n$out\n";
    exit(1);
}

$code = fj_run(escapeshellarg($bin), $out);
echo 'ilp32 guard probe: ', $code === 0 ? "PASS\n$out\n" : "FAIL\n$out\n";
@unlink($gen);
@unlink($bin);
exit($code === 0 ? 0 : 1);
?>
--EXPECTF--
ilp32 guard probe: PASS
unguarded new_cap*sizeof(u64) at cap=2^28 is 0
usize is 32 bits
PASS: %d checks, 0 failures
