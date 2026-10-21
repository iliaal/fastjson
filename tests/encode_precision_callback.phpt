--TEST--
Encoding observes serialize_precision changes inside JsonSerializable callbacks
--EXTENSIONS--
fastjson
--FILE--
<?php
class PrecisionValue implements JsonSerializable {
    private $precision;
    public function __construct($precision) { $this->precision = $precision; }
    public function jsonSerialize(): mixed {
        ini_set('serialize_precision', (string) $this->precision);
        return [1.23456789, '1.23456789', 100.0];
    }
}
$path = __DIR__ . '/precision-callback.json';
$cases = 0;
foreach ([-1, 3, 17] as $initial) {
    foreach ([-1, 3, 17] as $changed) {
        foreach ([0, JSON_NUMERIC_CHECK, JSON_PRESERVE_ZERO_FRACTION,
                  JSON_NUMERIC_CHECK | JSON_PRESERVE_ZERO_FRACTION] as $flags) {
            $value = [1.23456789, new PrecisionValue($changed), 1.23456789,
                      '1.23456789', new PrecisionValue($initial), 1.23456789];
            ini_set('serialize_precision', (string) $initial);
            $expected = json_encode($value, $flags);
            ini_set('serialize_precision', (string) $initial);
            if (fastjson_encode($value, $flags) !== $expected) {
                throw new Exception("encode mismatch: $initial -> $changed, flags=$flags");
            }
            ini_set('serialize_precision', (string) $initial);
            if (fastjson_file_encode($path, $value, $flags) === false || file_get_contents($path) !== $expected) {
                throw new Exception("file encode mismatch: $initial -> $changed, flags=$flags");
            }
            ini_set('serialize_precision', (string) $initial);
            if (fastjson_pointer_set('{"value":null}', '/value', $value, 512, $flags) !== '{"value":' . $expected . '}') {
                throw new Exception("pointer mismatch: $initial -> $changed, flags=$flags");
            }
            ++$cases;
        }
    }
}
unlink($path);
echo "$cases precision transitions passed\n";
?>
--CLEAN--
<?php @unlink(__DIR__ . '/precision-callback.json'); ?>
--EXPECT--
36 precision transitions passed
