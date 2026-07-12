--TEST--
fastjson_file_encode publishes its outer encode error after nested destructor encodes
--EXTENSIONS--
fastjson
--FILE--
<?php
final class FileSuccessTemporary {
    public int $value = 1;

    public function __destruct() {
        fastjson_encode(INF);
    }
}

final class FileSuccessOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new FileSuccessTemporary();
    }
}

$path = __DIR__ . '/file_encode_nested_error_state.tmp';
var_dump(fastjson_file_encode($path, new FileSuccessOuter()));
var_dump(file_get_contents($path));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);
unlink($path);
?>
--EXPECT--
bool(true)
string(11) "{"value":1}"
bool(true)
