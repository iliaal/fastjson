--TEST--
fastjson_file_decode: successful throw-mode decode restores the entry error state
--EXTENSIONS--
fastjson
--FILE--
<?php

final class PoisonOnCloseStream {
    public $context;
    public static string $contents = '{"ok":true}';
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
        fastjson_decode('bad');
    }

    public function stream_stat(): array {
        return [];
    }
}

stream_wrapper_register('fastjsonstatefile', PoisonOnCloseStream::class);
fastjson_decode('{"a": bad}');
$before = fastjson_last_error_info();
$result = fastjson_file_decode(
    'fastjsonstatefile://valid',
    true,
    512,
    JSON_THROW_ON_ERROR,
);
var_dump($result);
var_dump(fastjson_last_error_info() === $before);
stream_wrapper_unregister('fastjsonstatefile');
?>
--EXPECT--
array(1) {
  ["ok"]=>
  bool(true)
}
bool(true)
