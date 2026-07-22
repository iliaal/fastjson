--TEST--
fastjson_encode: hard errors keep traversing without buffering discarded output
--EXTENSIONS--
fastjson
--INI--
memory_limit=-1
--FILE--
<?php

class FastjsonHardErrorCallback implements JsonSerializable
{
    public static int $calls = 0;

    public function jsonSerialize(): mixed
    {
        self::$calls++;
        return true;
    }
}

$payload = [INF, str_repeat('a', 16 * 1024 * 1024), new FastjsonHardErrorCallback()];
$before = memory_get_peak_usage(true);
$result = fastjson_encode($payload);
$extraPeak = memory_get_peak_usage(true) - $before;

var_dump($result);
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);
var_dump(FastjsonHardErrorCallback::$calls);
var_dump($extraPeak < 4 * 1024 * 1024);

$associative = [
    'error' => INF,
    'callback' => new FastjsonHardErrorCallback(),
];
var_dump(fastjson_encode($associative));
var_dump(FastjsonHardErrorCallback::$calls);

$object = (object) [
    'error' => INF,
    'callback' => new FastjsonHardErrorCallback(),
];
var_dump(fastjson_encode($object));
var_dump(FastjsonHardErrorCallback::$calls);

var_dump(fastjson_encode(
    [INF, new FastjsonHardErrorCallback()],
    JSON_FORCE_OBJECT,
));
var_dump(FastjsonHardErrorCallback::$calls);
?>
--EXPECT--
bool(false)
bool(true)
int(1)
bool(true)
bool(false)
int(2)
bool(false)
int(3)
bool(false)
int(4)
