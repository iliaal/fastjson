--TEST--
fastjson_file_decode stops when a stream wrapper throws during close
--EXTENSIONS--
fastjson
--FILE--
<?php

final class ThrowingCloseStream {
    public static string $contents = '';
    public $context;
    private int $offset = 0;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        return true;
    }

    public function stream_read(int $count): string {
        $chunk = substr(self::$contents, $this->offset, $count);
        $this->offset += strlen($chunk);
        return $chunk;
    }

    public function stream_eof(): bool {
        return $this->offset >= strlen(self::$contents);
    }

    public function stream_close(): void {
        throw new RuntimeException('close-boom');
    }

    public function stream_stat(): array { return []; }
}

stream_wrapper_register('fastjsonclose', ThrowingCloseStream::class);

ThrowingCloseStream::$contents = '{"ok":true}';
try {
    fastjson_file_decode('fastjsonclose://valid', true);
} catch (Throwable $exception) {
    echo get_class($exception), ': ', $exception->getMessage(), "\n";
}
var_dump(fastjson_last_error());

fastjson_encode(INF);
$savedError = fastjson_last_error();
ThrowingCloseStream::$contents = '{';
try {
    fastjson_file_decode(
        'fastjsonclose://invalid',
        true,
        512,
        JSON_THROW_ON_ERROR,
    );
} catch (Throwable $exception) {
    echo get_class($exception), ': ', $exception->getMessage(), "\n";
    var_dump($exception->getPrevious());
}
var_dump(fastjson_last_error() === $savedError);

stream_wrapper_unregister('fastjsonclose');
?>
--EXPECT--
RuntimeException: close-boom
int(0)
RuntimeException: close-boom
NULL
bool(true)
