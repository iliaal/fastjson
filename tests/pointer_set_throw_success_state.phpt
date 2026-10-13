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

foreach ([false, true] as $seedError) {
    foreach ([2, new PoisonOnSerialize()] as $value) {
        fastjson_decode($seedError ? "{\n  \"seed\": bad\n}" : '0');
        $before = fastjson_last_error_info();
        if ($seedError && ($before['pos'] <= 0 || $before['line'] <= 1 || $before['col'] <= 1)) {
            throw new RuntimeException('seed did not carry error location');
        }
        $result = fastjson_pointer_set('{"a":1}', '/a', $value, 512, JSON_THROW_ON_ERROR);
        if ($result !== '{"a":2}' || fastjson_last_error_info() !== $before) {
            throw new RuntimeException('throw-mode success changed result or entry error state');
        }
    }
}
echo "success matrix ok\n";

$before = fastjson_last_error_info();
try {
    fastjson_pointer_set('{"a":1}', '/a', NAN, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $exception) {
    echo $exception->getCode(), "\n";
}
var_dump(fastjson_last_error_info() === $before);
?>
--EXPECT--
success matrix ok
7
bool(true)
