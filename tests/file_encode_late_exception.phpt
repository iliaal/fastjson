--TEST--
fastjson_file_encode does not write bytes after a temporary destructor throws
--EXTENSIONS--
fastjson
--FILE--
<?php

final class FastjsonThrowingTemporary {
    public int $value = 1;
    public function __destruct() {
        throw new RuntimeException('late-destructor');
    }
}

final class FastjsonLateOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new FastjsonThrowingTemporary();
    }
}

$path = __DIR__ . '/file_encode_late_exception.tmp';
@unlink($path);
try {
    fastjson_file_encode($path, new FastjsonLateOuter());
    echo "no exception\n";
} catch (Throwable $exception) {
    echo get_class($exception), ': ', $exception->getMessage(), "\n";
}
var_dump(file_exists($path));
?>
--EXPECT--
RuntimeException: late-destructor
bool(false)
