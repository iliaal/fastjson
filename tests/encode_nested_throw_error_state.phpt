--TEST--
fastjson_encode throws the outer error when a nested destructor encode changes global state
--EXTENSIONS--
fastjson
--FILE--
<?php
function nativeThrowDestructorEncode(): void {
    $stream = fopen('php://memory', 'r');
    json_encode($stream);
    fclose($stream);
}

function fastThrowDestructorEncode(): void {
    $stream = fopen('php://memory', 'r');
    fastjson_encode($stream);
    fclose($stream);
}

final class NativeThrowFailureTemporary {
    public string $value = "\xFF";

    public function __destruct() {
        nativeThrowDestructorEncode();
    }
}

final class NativeThrowFailureOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new NativeThrowFailureTemporary();
    }
}

final class FastThrowFailureTemporary {
    public string $value = "\xFF";

    public function __destruct() {
        fastThrowDestructorEncode();
    }
}

final class FastThrowFailureOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new FastThrowFailureTemporary();
    }
}

try {
    json_encode(new NativeThrowFailureOuter(), JSON_THROW_ON_ERROR);
} catch (JsonException $exception) {
    var_dump($exception->getCode());
}

fastjson_decode('{');
$fastSavedError = fastjson_last_error();
try {
    fastjson_encode(new FastThrowFailureOuter(), JSON_THROW_ON_ERROR);
} catch (JsonException $exception) {
    var_dump($exception->getCode());
}
var_dump(fastjson_last_error() === $fastSavedError);
?>
--EXPECT--
int(5)
int(5)
bool(true)
