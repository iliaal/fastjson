--TEST--
Negative zero honors serialize_precision across encoding entry points
--EXTENSIONS--
fastjson
--FILE--
<?php
$path = __DIR__ . '/negative-zero-precision.json';
$cases = 0;
foreach ([-1, 0, 1, 2, 17] as $precision) {
    ini_set('serialize_precision', (string) $precision);
    foreach ([0, JSON_PRESERVE_ZERO_FRACTION, JSON_NUMERIC_CHECK,
              JSON_NUMERIC_CHECK | JSON_PRESERVE_ZERO_FRACTION] as $flags) {
        foreach ([-0.0, [-0.0, 0.0, -0.01, '-0.0']] as $value) {
            $expected = json_encode($value, $flags);
            if (fastjson_encode($value, $flags) !== $expected) {
                throw new Exception("encode mismatch: precision=$precision flags=$flags");
            }
            if (fastjson_file_encode($path, $value, $flags) === false || file_get_contents($path) !== $expected) {
                throw new Exception("file mismatch: precision=$precision flags=$flags");
            }
            if (fastjson_pointer_set('{"value":null}', '/value', $value, 512, $flags) !== '{"value":' . $expected . '}') {
                throw new Exception("pointer mismatch: precision=$precision flags=$flags");
            }
            ++$cases;
        }
    }
}
class ZeroPrecision implements JsonSerializable {
    public function jsonSerialize(): mixed {
        ini_set('serialize_precision', '0');
        return -0.0;
    }
}
foreach ([0, JSON_PRESERVE_ZERO_FRACTION] as $flags) {
    $value = [new ZeroPrecision(), -0.0];
    ini_set('serialize_precision', '-1');
    $expected = json_encode($value, $flags);
    ini_set('serialize_precision', '-1');
    if (fastjson_encode($value, $flags) !== $expected) {
        throw new Exception("callback mismatch: flags=$flags");
    }
}
unlink($path);
echo "$cases negative-zero cases passed\n";
?>
--CLEAN--
<?php @unlink(__DIR__ . '/negative-zero-precision.json'); ?>
--EXPECT--
40 negative-zero cases passed
