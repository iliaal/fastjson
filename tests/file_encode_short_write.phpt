--TEST--
fastjson_file_encode loops short stream writes to completion and reports zero-progress writes
--EXTENSIONS--
fastjson
--FILE--
<?php
final class ShortWriteStream {
    public $context;
    public static string $data = '';

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        self::$data = '';
        return true;
    }

    public function stream_write(string $data): int {
        self::$data .= $data[0];
        return 1;
    }

    public function stream_close(): void {}
    public function stream_stat(): array { return []; }
}

final class ZeroWriteStream {
    public $context;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        return true;
    }

    public function stream_write(string $data): int {
        return 0;
    }

    public function stream_close(): void {}
    public function stream_stat(): array { return []; }
}

stream_wrapper_register('fjshort', ShortWriteStream::class);
stream_wrapper_register('fjzero', ZeroWriteStream::class);

$data = ['k' => 'v/a', 'n' => 42, 'list' => [1, 2, 3], 'nested' => ['x' => true]];
$expected = fastjson_encode($data);

var_dump(fastjson_file_encode('fjshort://sink', $data));
var_dump(ShortWriteStream::$data === $expected);

var_dump(fastjson_file_encode('fjzero://sink', $data));
var_dump(fastjson_last_error() !== JSON_ERROR_NONE);

stream_wrapper_unregister('fjshort');
stream_wrapper_unregister('fjzero');
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
