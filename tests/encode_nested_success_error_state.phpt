--TEST--
fastjson_encode publishes the successful outer error state after a nested destructor encode
--EXTENSIONS--
fastjson
--FILE--
<?php
final class NativeSuccessTemporary {
    public int $value = 1;

    public function __destruct() {
        json_encode(INF);
    }
}

final class NativeSuccessOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new NativeSuccessTemporary();
    }
}

final class FastSuccessTemporary {
    public int $value = 1;

    public function __destruct() {
        fastjson_encode(INF);
    }
}

final class FastSuccessOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new FastSuccessTemporary();
    }
}

$native = json_encode(new NativeSuccessOuter());
$nativeError = json_last_error();
$fast = fastjson_encode(new FastSuccessOuter());
$fastError = fastjson_last_error();

var_dump($fast === $native);
var_dump($fastError === $nativeError);
var_dump($fastError === JSON_ERROR_NONE);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
