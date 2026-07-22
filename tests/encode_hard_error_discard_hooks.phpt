--TEST--
fastjson_encode: hard-error traversal still reads later property hooks
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID < 80400) die('skip property hooks require PHP 8.4'); ?>
--INI--
memory_limit=-1
--FILE--
<?php

class FastjsonDiscardHooked
{
    public static int $calls = 0;
    public float $error = INF;
    public string $tail;
    public int $value {
        get {
            self::$calls++;
            return 42;
        }
    }

    public function __construct()
    {
        $this->tail = str_repeat('a', 16 * 1024 * 1024);
    }
}

$value = new FastjsonDiscardHooked();
$before = memory_get_peak_usage(true);
$result = fastjson_encode($value);
$extraPeak = memory_get_peak_usage(true) - $before;

var_dump($result);
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);
var_dump(FastjsonDiscardHooked::$calls);
var_dump($extraPeak < 4 * 1024 * 1024);
?>
--EXPECT--
bool(false)
bool(true)
int(1)
bool(true)
