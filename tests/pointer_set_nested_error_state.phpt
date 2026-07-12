--TEST--
fastjson_pointer_set publishes its replacement encode error after a nested destructor encode
--EXTENSIONS--
fastjson
--FILE--
<?php
final class PointerSuccessTemporary {
    public int $value = 1;

    public function __destruct() {
        fastjson_encode(INF);
    }
}

final class PointerSuccessOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new PointerSuccessTemporary();
    }
}

var_dump(fastjson_pointer_set('{}', '/item', new PointerSuccessOuter()));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);
?>
--EXPECT--
string(20) "{"item":{"value":1}}"
bool(true)
