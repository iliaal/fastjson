--TEST--
fastjson_decode/file_decode/pointer_set: shared entry validation preserves depth boundaries and complete error state
--EXTENSIONS--
fastjson
--FILE--
<?php
$path = __DIR__ . '/decode_entry_state.json';
file_put_contents($path, '42');
fastjson_decode('42');
$clear = fastjson_last_error_info();
$depths = [-1, 0, 1, 2147483646, 2147483647];
if (PHP_INT_SIZE > 4) {
    $depths[] = 2147483648;
}
$cases = [
    ['fastjson_decode', 3, 42, static fn($depth, $flags) => fastjson_decode('42', null, $depth, $flags)],
    ['fastjson_file_decode', 3, 42, static fn($depth, $flags) => fastjson_file_decode($path, null, $depth, $flags)],
    ['fastjson_pointer_set', 4, '42', static fn($depth, $flags) => fastjson_pointer_set('0', '', 42, $depth, $flags)],
];
foreach ($cases as [$fn, $argno, $expectedResult, $call]) {
    foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
        foreach ($depths as $depth) {
            fastjson_decode("[\n  1,\n  ?]");
            $before = fastjson_last_error_info();
            if ($before['pos'] < 0 || $before['line'] < 2 || $before['col'] < 1) {
                throw new Exception('missing seeded error coordinates');
            }
            $invalid = $depth <= 0 || $depth > 2147483647;
            try {
                $result = $call($depth, $flags);
                if ($invalid || $result !== $expectedResult) {
                    throw new Exception("unexpected result from $fn at depth $depth");
                }
            } catch (ValueError $e) {
                $reason = $depth <= 0 ? 'greater than 0' : 'less than 2147483647';
                $message = $fn . '(): Argument #' . $argno . ' ($depth) must be ' . $reason;
                if (!$invalid || $e->getMessage() !== $message) {
                    throw new Exception("unexpected depth diagnostic: " . $e->getMessage());
                }
            }
            $expected = $flags ? $before : $clear;
            if (fastjson_last_error_info() !== $expected) {
                throw new Exception("unexpected error state from $fn at depth $depth");
            }
        }
    }
}
// Invalid depth must reject before a replacement callback can run.
class DepthReplacementProbe implements JsonSerializable {
    public static int $calls = 0;
    public function jsonSerialize(): mixed {
        self::$calls++;
        return 42;
    }
}
foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
    foreach ($depths as $depth) {
        if ($depth > 0 && $depth <= 2147483647) continue;
        try {
            fastjson_pointer_set('0', '', new DepthReplacementProbe(), $depth, $flags);
            throw new Exception('invalid depth accepted');
        } catch (ValueError $e) {
        }
    }
}
if (DepthReplacementProbe::$calls !== 0) {
    throw new Exception('replacement callback ran before depth validation');
}
unlink($path);
echo "decode entry contracts passed\n";
?>
--CLEAN--
<?php @unlink(__DIR__ . '/decode_entry_state.json'); ?>
--EXPECT--
decode entry contracts passed
