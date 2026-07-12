--TEST--
fastjson_file_encode keeps the current non-throw error when a stream wrapper throws
--EXTENSIONS--
fastjson
--FILE--
<?php

final class ThrowingWriteStream {
    public $context;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        return true;
    }

    public function stream_write(string $data): int {
        throw new RuntimeException('write-boom');
    }

    public function stream_close(): void {}
    public function stream_stat(): array { return []; }
}

stream_wrapper_register('fastjsonwrite', ThrowingWriteStream::class);
fastjson_decode('{');

try {
    fastjson_file_encode(
        'fastjsonwrite://sink',
        INF,
        JSON_PARTIAL_OUTPUT_ON_ERROR,
    );
} catch (Throwable $exception) {
    echo get_class($exception), ': ', $exception->getMessage(), "\n";
}

var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);
stream_wrapper_unregister('fastjsonwrite');
?>
--EXPECT--
RuntimeException: write-boom
bool(true)
