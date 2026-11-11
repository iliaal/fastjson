--TEST--
Decode readers share parse diagnostics while preserving throw state and failure return types
--EXTENSIONS--
fastjson
json
--INI--
allow_url_fopen=1
--FILE--
<?php
$path = __DIR__ . '/read_failure_contract.json';
$readers = [
    'decode' => [static fn($json, $flags) => fastjson_decode($json, null, 512, $flags), null],
    'file' => [static fn($json, $flags) => fastjson_file_decode($path, null, 512, $flags), null],
    'stream' => [static fn($json, $flags) => fastjson_file_decode(
        'data://text/plain;base64,' . base64_encode($json), null, 512, $flags), null],
    'get' => [static fn($json, $flags) => fastjson_pointer_get($json, '', null, 512, $flags), null],
    'exists' => [static fn($json, $flags) => fastjson_pointer_exists($json, '', $flags), false],
    'set' => [static fn($json, $flags) => fastjson_pointer_set($json, '', 42, 512, $flags), false],
    'merge-target' => [static fn($json, $flags) => fastjson_merge_patch($json, '{}', null, 512, $flags), null],
    'merge-patch' => [static fn($json, $flags) => fastjson_merge_patch('{}', $json, null, 512, $flags), null],
    // Both operands fail differently: the target diagnostic must win.
    'merge-both' => [static fn($json, $flags) => fastjson_merge_patch($json, "[\"\xFF\"]", null, 512, $flags), null],
];
$cases = [
    ['', 0, JSON_ERROR_SYNTAX, 'input length is 0', 0, 1, 1],
    ["\n[?]", 0, JSON_ERROR_SYNTAX, 'unexpected character, expected a JSON value', 2, 2, 2],
    ['{"x":1,}', 0, JSON_ERROR_SYNTAX, 'trailing comma is not allowed', 6, 1, 7],
    ["[\"\xFF\"]", 0, JSON_ERROR_UTF8, 'invalid UTF-8 encoding in string', 2, 1, 3],
    ['NaN', 0, JSON_ERROR_SYNTAX, 'unexpected character, expected a JSON value', 0, 1, 1],
    ['[1e309,?]', 0, JSON_ERROR_SYNTAX, 'unexpected character, expected a JSON value', 7, 1, 8],
    ["\xEF\xBB\xBF[\n?]", 0, JSON_ERROR_SYNTAX, 'UTF-8 byte order mark (BOM) is not supported', 0, 1, 1],
    ["\xEF\xBB\xBF[\n?]", FASTJSON_DECODE_RELAXED, JSON_ERROR_SYNTAX, 'unexpected character, expected a JSON value', 5, 2, 1],
];
$count = 0;
try {
    foreach ($cases as [$json, $flags, $code, $msg, $pos, $line, $col]) {
        file_put_contents($path, $json);
        foreach ($readers as $name => [$read, $failure]) {
            foreach ([false, true] as $throw) {
                fastjson_decode("\n\n[0,?]");
                $before = fastjson_last_error_info();
                try {
                    $result = $read($json, $flags | ($throw ? JSON_THROW_ON_ERROR : 0));
                    if ($throw || $result !== $failure) {
                        throw new RuntimeException("$name returned an unexpected result");
                    }
                } catch (JsonException $e) {
                    if (!$throw || $e->getCode() !== $code || $e->getMessage() !== $msg) {
                        throw new RuntimeException("$name threw an unexpected diagnostic");
                    }
                }
                $expected = $throw ? $before : compact('code', 'msg', 'pos', 'line', 'col');
                if (fastjson_last_error_info() !== $expected) {
                    throw new RuntimeException("$name changed the complete error state");
                }
                $count++;
            }
        }
    }
} finally {
    unlink($path);
}
echo "$count reader failure contracts passed\n";
// Pointer-set must retain raw numeric spellings and mask decode-only bits,
// which overlap encode flags.
foreach ([0, JSON_THROW_ON_ERROR] as $throw) {
    $result = fastjson_pointer_set('{"n":1e+02,"big":18446744073709551616,"x":0}',
        '/x', 42, 512, $throw | JSON_HEX_TAG | JSON_HEX_AMP);
    if ($result !== '{"n":1e+02,"big":18446744073709551616,"x":42}') {
        throw new RuntimeException('pointer-set lost raw numeric spellings');
    }
}
foreach ([JSON_INVALID_UTF8_IGNORE, JSON_INVALID_UTF8_SUBSTITUTE,
          JSON_INVALID_UTF8_IGNORE | JSON_INVALID_UTF8_SUBSTITUTE] as $handling) {
    foreach ([false, true] as $throw) {
        fastjson_decode("\n\n[0,?]");
        $before = fastjson_last_error_info();
        try {
            $result = fastjson_pointer_set("[\"\xFF\"]", '', 42, 512,
                $handling | ($throw ? JSON_THROW_ON_ERROR : 0));
            if ($throw || $result !== false) {
                throw new RuntimeException('pointer-set accepted malformed base UTF-8');
            }
        } catch (JsonException $e) {
            if (!$throw || $e->getCode() !== JSON_ERROR_UTF8
                    || $e->getMessage() !== 'invalid UTF-8 encoding in string') {
                throw new RuntimeException('pointer-set changed its base UTF-8 diagnostic');
            }
        }
        $expected = $throw ? $before : [
            'code' => JSON_ERROR_UTF8, 'msg' => 'invalid UTF-8 encoding in string',
            'pos' => 2, 'line' => 1, 'col' => 3,
        ];
        if (fastjson_last_error_info() !== $expected) {
            throw new RuntimeException('pointer-set changed its base UTF-8 error state');
        }
    }
}
echo "pointer-set parse flags preserved\n";

?>
--CLEAN--
<?php @unlink(__DIR__ . '/read_failure_contract.json'); ?>
--EXPECT--
144 reader failure contracts passed
pointer-set parse flags preserved
