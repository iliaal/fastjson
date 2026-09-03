--TEST--
fastjson_encode: post-hard-error discard never re-invokes JsonSerializable and preserves the first error
--EXTENSIONS--
fastjson
--FILE--
<?php
class CountingSerializer implements JsonSerializable {
    public static int $calls = 0;

    public function jsonSerialize(): mixed {
        self::$calls++;
        return [INF];
    }
}

$result = fastjson_encode([new CountingSerializer(), 'ok' => true]);
var_dump($result);
var_dump(CountingSerializer::$calls);
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);

CountingSerializer::$calls = 0;
var_dump(fastjson_encode(new CountingSerializer()));
var_dump(CountingSerializer::$calls);
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);

json_encode([new CountingSerializer(), 'ok' => true]);
var_dump(json_last_error() === fastjson_last_error());
?>
--EXPECT--
bool(false)
int(1)
bool(true)
bool(false)
int(1)
bool(true)
bool(true)
