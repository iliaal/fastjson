--TEST--
fastjson_pointer_set: reject invalid locations before invoking replacement callbacks
--EXTENSIONS--
fastjson
--FILE--
<?php

class PointerEffect implements JsonSerializable {
    public static int $calls = 0;
    public function jsonSerialize(): mixed {
        self::$calls++;
        return ['value' => self::$calls];
    }
}

$value = new PointerEffect();

var_dump(fastjson_pointer_set('{}', 'invalid', $value));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
var_dump(PointerEffect::$calls);

var_dump(fastjson_pointer_set('{"a":1}', '/a/b', $value));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
var_dump(PointerEffect::$calls);

var_dump(fastjson_pointer_set('{"a":1,"a":2}', '/a', $value));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
var_dump(PointerEffect::$calls);

echo fastjson_pointer_set('{}', '/ok', $value), "\n";
var_dump(PointerEffect::$calls);

class PointerBoom implements JsonSerializable {
    public function jsonSerialize(): mixed {
        throw new RuntimeException('called');
    }
}

var_dump(fastjson_pointer_set('{}', 'invalid', new PointerBoom()));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

try {
    fastjson_pointer_set('{}', '/ok', new PointerBoom());
} catch (RuntimeException $e) {
    echo $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(false)
bool(true)
int(0)
bool(false)
bool(true)
int(0)
bool(false)
bool(true)
int(0)
{"ok":{"value":1}}
int(1)
bool(false)
bool(true)
called
