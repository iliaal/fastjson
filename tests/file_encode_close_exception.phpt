--TEST--
fastjson_file_encode stops when a stream wrapper throws during close
--EXTENSIONS--
fastjson
--FILE--
<?php
final class ThrowingEncodeCloseStream {
    public $context;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        return true;
    }

    public function stream_write(string $data): int {
        return strlen($data);
    }

    public function stream_close(): void {
        throw new RuntimeException('close-boom');
    }

    public function stream_stat(): array {
        return [];
    }
}

stream_wrapper_register('fastjsonencodeclose', ThrowingEncodeCloseStream::class);

try {
    fastjson_file_encode('fastjsonencodeclose://nonthrow', ['ok' => true]);
} catch (Throwable $exception) {
    echo get_class($exception), ': ', $exception->getMessage(), "\n";
}
var_dump(fastjson_last_error());

fastjson_decode('{');
$savedError = fastjson_last_error();
try {
    fastjson_file_encode(
        'fastjsonencodeclose://throw',
        ['ok' => true],
        JSON_THROW_ON_ERROR,
    );
} catch (Throwable $exception) {
    echo get_class($exception), ': ', $exception->getMessage(), "\n";
}
var_dump(fastjson_last_error() === $savedError);

stream_wrapper_unregister('fastjsonencodeclose');
?>
--EXPECT--
RuntimeException: close-boom
int(0)
RuntimeException: close-boom
bool(true)
