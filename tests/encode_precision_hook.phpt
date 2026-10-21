--TEST--
Encoding observes serialize_precision changes inside property hooks
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID < 80400) die('skip property hooks require PHP 8.4+'); ?>
--FILE--
<?php
class PrecisionHook {
    public float $number {
        get {
            ini_set('serialize_precision', '3');
            return 1.23456789;
        }
    }
    public string $numeric {
        get {
            ini_set('serialize_precision', '5');
            return '1.23456789';
        }
    }
}
foreach ([0, JSON_NUMERIC_CHECK, JSON_PRESERVE_ZERO_FRACTION] as $flags) {
    ini_set('serialize_precision', '-1');
    $expected = json_encode([new PrecisionHook, 1.23456789], $flags);
    ini_set('serialize_precision', '-1');
    var_dump(fastjson_encode([new PrecisionHook, 1.23456789], $flags) === $expected);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
