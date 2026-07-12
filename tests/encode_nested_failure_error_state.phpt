--TEST--
fastjson_encode publishes the failed outer error after a nested destructor encode
--EXTENSIONS--
fastjson
--FILE--
<?php
function nativeFailureDestructorEncode(): void {
    $stream = fopen('php://memory', 'r');
    json_encode($stream);
    fclose($stream);
}

function fastFailureDestructorEncode(): void {
    $stream = fopen('php://memory', 'r');
    fastjson_encode($stream);
    fclose($stream);
}

final class NativeFailureTemporary {
    public string $value = "\xFF";

    public function __destruct() {
        nativeFailureDestructorEncode();
    }
}

final class NativeFailureOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new NativeFailureTemporary();
    }
}

final class FastFailureTemporary {
    public string $value = "\xFF";

    public function __destruct() {
        fastFailureDestructorEncode();
    }
}

final class FastFailureOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new FastFailureTemporary();
    }
}

$native = json_encode(new NativeFailureOuter());
$nativeError = json_last_error();
$fast = fastjson_encode(new FastFailureOuter());
$fastError = fastjson_last_error();

var_dump($fast === $native);
var_dump($fastError === $nativeError);
var_dump($fastError === JSON_ERROR_UTF8);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
