--TEST--
fastjson_pointer_set: successful throw-mode set restores the entry error state
--EXTENSIONS--
fastjson
--FILE--
<?php

final class PoisonOnSerialize implements JsonSerializable {
    public function jsonSerialize(): mixed {
        fastjson_decode('bad');
        return 2;
    }
}

fastjson_decode('{"a": bad}');
$before = fastjson_last_error_info();
$result = fastjson_pointer_set(
    '{"a":1}',
    '/a',
    new PoisonOnSerialize(),
    512,
    JSON_THROW_ON_ERROR,
);
var_dump($result);
var_dump(fastjson_last_error_info() === $before);

$before = fastjson_last_error_info();
try {
    fastjson_pointer_set('{"a":1}', '/a', NAN, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $exception) {
    echo $exception->getCode(), "\n";
}
var_dump(fastjson_last_error_info() === $before);
?>
--EXPECT--
string(7) "{"a":2}"
bool(true)
7
bool(true)
