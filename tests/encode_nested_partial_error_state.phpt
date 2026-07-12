--TEST--
fastjson_encode publishes the partial-output outer error after a nested destructor encode
--EXTENSIONS--
fastjson
--FILE--
<?php
function nativeUnsupportedEncode(): void {
    $stream = fopen('php://memory', 'r');
    json_encode($stream);
    fclose($stream);
}

function fastUnsupportedEncode(): void {
    $stream = fopen('php://memory', 'r');
    fastjson_encode($stream);
    fclose($stream);
}

final class NativePartialTemporary {
    public float $value = INF;

    public function __destruct() {
        nativeUnsupportedEncode();
    }
}

final class NativePartialOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new NativePartialTemporary();
    }
}

final class FastPartialTemporary {
    public float $value = INF;

    public function __destruct() {
        fastUnsupportedEncode();
    }
}

final class FastPartialOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new FastPartialTemporary();
    }
}

$native = json_encode(new NativePartialOuter(), JSON_PARTIAL_OUTPUT_ON_ERROR);
$nativeError = json_last_error();
$fast = fastjson_encode(new FastPartialOuter(), JSON_PARTIAL_OUTPUT_ON_ERROR);
$fastError = fastjson_last_error();

var_dump($fast === $native);
var_dump($fastError === $nativeError);
var_dump($fastError === JSON_ERROR_INF_OR_NAN);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
