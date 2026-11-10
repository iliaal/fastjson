--TEST--
Parse error coordinates survive ASCII word boundaries and UTF-8/BOM fallback across readers
--EXTENSIONS--
fastjson
--INI--
allow_url_fopen=1
--FILE--
<?php
// Each prefix is valid up to either an invalid value marker or EOF. Expected
// line/column values come from construction, not another call to the locator.
$cases = [];
for ($pad = 0; $pad < 24; $pad++) {
    $prefix = '[' . str_repeat(' ', $pad);
    $cases[] = [$prefix, 1, $pad + 2];
    // CR and tab are JSON whitespace but do not start a new line here.
    $cases[] = [$prefix . "\r\t", 1, $pad + 4];
    foreach ([0, 1, 7, 8, 9] as $tail) {
        $spaces = str_repeat(' ', $tail);
        $cases[] = [$prefix . "\n" . $spaces, 2, $tail + 1];
        $cases[] = [$prefix . "\n\r\t\n" . $spaces, 3, $tail + 1];
    }
}
for ($pad = 0; $pad < 16; $pad++) {
    foreach (["\u{00e9}", "\u{20ac}", "\u{1f600}"] as $character) {
        // Move 2-, 3-, and 4-byte characters through word and tail positions.
        $prefix = '[' . str_repeat(' ', $pad) . '"' . $character . '",';
        $cases[] = [$prefix, 1, $pad + 6];
        $cases[] = [$prefix . "\n ", 2, 2];
    }
}
for ($pad = 0; $pad < 24; $pad++) {
    // The offending LF is not part of the preceding line/column scan. Unlike
    // a malformed value, an unescaped string control byte maps to UTF8.
    $cases[] = ['["' . str_repeat(' ', $pad), 1, $pad + 3, ["\n\"]"], JSON_ERROR_UTF8];
}

$file = tempnam(sys_get_temp_dir(), 'fastjson-coordinates-');
if ($file === false) throw new RuntimeException('cannot create input file');
$readers = [
    'decode' => [static fn($json, $flags) => fastjson_decode($json, null, 512, $flags), null],
    'validate' => [static fn($json, $flags) => fastjson_validate($json), false],
    'pointer-get' => [static fn($json, $flags) => fastjson_pointer_get($json, '/0', null, 512, $flags), null],
    'pointer-exists' => [static fn($json, $flags) => fastjson_pointer_exists($json, '/0', $flags), false],
    'pointer-set' => [static fn($json, $flags) => fastjson_pointer_set($json, '/0', 1, 512, $flags), false],
    'merge-target' => [static fn($json, $flags) => fastjson_merge_patch($json, '{}', null, 512, $flags), null],
    'merge-patch' => [static fn($json, $flags) => fastjson_merge_patch('{}', $json, null, 512, $flags), null],
    'file' => [static fn($json, $flags) => fastjson_file_decode($file, null, 512, $flags), null],
    'data-stream' => [static fn($json, $flags) => fastjson_file_decode(
        'data://text/plain;base64,' . base64_encode($json), null, 512, $flags), null],
];
$counts = array_fill_keys(array_keys($readers), 0);
try {
    foreach ($cases as $index => $case) {
        [$prefix, $line, $col] = $case;
        foreach ([0, FASTJSON_DECODE_RELAXED] as $flags) {
            $input = ($flags ? "\xEF\xBB\xBF" : '') . $prefix;
            foreach ($case[3] ?? ['@]', ''] as $suffix) {
                $json = $input . $suffix;
                if (file_put_contents($file, $json) !== strlen($json)) {
                    throw new RuntimeException('cannot write input file');
                }
                foreach ($readers as $name => [$read, $failure]) {
                    // Validate accepts only strict JSON, not the relaxed flag.
                    if ($name === 'validate' && $flags) continue;
                    // Seed a different location so stale state cannot pass.
                    fastjson_decode("\n\n@");
                    $result = $read($json, $flags);
                    $info = fastjson_last_error_info();
                    if ($result !== $failure || $info['code'] !== ($case[4] ?? JSON_ERROR_SYNTAX)
                            || [$info['pos'], $info['line'], $info['col']]
                                !== [strlen($input), $line, $col]
                            || $info['code'] !== fastjson_last_error()
                            || $info['msg'] !== fastjson_last_error_msg()
                            || $info['pos'] !== fastjson_last_error_pos()) {
                        throw new RuntimeException("$name case $index flags $flags suffix "
                            . json_encode($suffix) . ': ' . json_encode($info));
                    }
                    $counts[$name]++;
                }
            }
        }
    }
} finally {
    unlink($file);
}
echo count($cases), " constructed prefixes\n";
foreach ($counts as $name => $count) echo "$name: $count coordinate checks\n";
?>
--EXPECT--
408 constructed prefixes
decode: 1584 coordinate checks
validate: 792 coordinate checks
pointer-get: 1584 coordinate checks
pointer-exists: 1584 coordinate checks
pointer-set: 1584 coordinate checks
merge-target: 1584 coordinate checks
merge-patch: 1584 coordinate checks
file: 1584 coordinate checks
data-stream: 1584 coordinate checks
