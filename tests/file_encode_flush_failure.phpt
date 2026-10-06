--TEST--
fastjson_file_encode reports flush failures and preserves wrapper exceptions
--EXTENSIONS--
fastjson
--FILE--
<?php
final class FlushFailureStream {
    public $context;
    public static bool $closed = false;
    public static bool $throw = false;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        self::$closed = false;
        return true;
    }

    public function stream_write(string $data): int {
        return strlen($data);
    }

    public function stream_flush(): bool {
        if (self::$throw) {
            throw new RuntimeException('flush-boom');
        }
        return false;
    }

    public function stream_close(): void {
        self::$closed = true;
    }
}

stream_wrapper_register('fjflushfail', FlushFailureStream::class);

// I/O failures return false even with JSON_THROW_ON_ERROR.
foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
    var_dump(fastjson_file_encode('fjflushfail://sink', ['ok' => true], $flags));
    var_dump(FlushFailureStream::$closed);
    var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
    echo fastjson_last_error_msg(), "\n";
}

FlushFailureStream::$throw = true;
foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
    fastjson_decode('{');
    $saved = fastjson_last_error_info();
    try {
        fastjson_file_encode('fjflushfail://sink', ['ok' => true], $flags);
    } catch (RuntimeException $exception) {
        echo $exception->getMessage(), "\n";
    }
    var_dump($flags ? fastjson_last_error_info() === $saved : fastjson_last_error() === JSON_ERROR_NONE);
}

stream_wrapper_unregister('fjflushfail');

// An omitted optional callback is not a failed flush.
class NoFlushStream {
    public $context;
    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        return true;
    }
    public function stream_write(string $data): int { return strlen($data); }
    public function stream_close(): void {}
}

// __call can provide the optional callback and must not be bypassed.
final class MagicFlushStream extends NoFlushStream {
    public function __call(string $name, array $arguments): mixed {
        return false;
    }
}

stream_wrapper_register('fjnoflush', NoFlushStream::class);
stream_wrapper_register('fjmagicflush', MagicFlushStream::class);
foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
    var_dump(fastjson_file_encode('fjnoflush://sink', [1], $flags));
    var_dump(fastjson_file_encode('fjmagicflush://sink', [1], $flags));
    var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
}
stream_wrapper_unregister('fjnoflush');
stream_wrapper_unregister('fjmagicflush');

final class SelfClosingFlushStream extends NoFlushStream {
    public function stream_eof(): bool { return false; }
    public function stream_flush(): bool {
        foreach (get_resources('stream') as $stream) {
            $metadata = stream_get_meta_data($stream);
            if (($metadata['wrapper_data'] ?? null) === $this) {
                try {
                    @fclose($stream);
                } catch (Throwable $exception) {
                    // PHP versions differ in how they reject the close.
                }
                var_dump(is_resource($stream));
            }
        }
        return true;
    }
}
stream_wrapper_register('fjselfclose', SelfClosingFlushStream::class);
var_dump(fastjson_file_encode('fjselfclose://sink', [1]));
stream_wrapper_unregister('fjselfclose');


?>
--EXPECT--
bool(false)
bool(true)
bool(true)
Failed to write file
bool(false)
bool(true)
bool(true)
Failed to write file
flush-boom
bool(true)
flush-boom
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
