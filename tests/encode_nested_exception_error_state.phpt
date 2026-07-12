--TEST--
fastjson_encode matches outer error publication when a nested callback throws
--EXTENSIONS--
fastjson
--FILE--
<?php
final class NativeThrowingOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        json_encode(INF);
        throw new RuntimeException('native-boom');
    }
}

final class FastThrowingOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        fastjson_encode(INF);
        throw new RuntimeException('fast-boom');
    }
}

try {
    json_encode(new NativeThrowingOuter());
} catch (RuntimeException) {
}
$nativeError = json_last_error();

try {
    fastjson_encode(new FastThrowingOuter());
} catch (RuntimeException) {
}
$fastError = fastjson_last_error();

var_dump($fastError === $nativeError);
var_dump($fastError === JSON_ERROR_NONE);

try {
    json_encode(new NativeThrowingOuter(), JSON_THROW_ON_ERROR);
} catch (RuntimeException) {
}
$nativeThrowError = json_last_error();

try {
    fastjson_encode(new FastThrowingOuter(), JSON_THROW_ON_ERROR);
} catch (RuntimeException) {
}
$fastThrowError = fastjson_last_error();

var_dump($fastThrowError === $nativeThrowError);
var_dump($fastThrowError === JSON_ERROR_INF_OR_NAN);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
