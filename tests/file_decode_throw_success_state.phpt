--TEST--
fastjson_file_decode: successful throw-mode decode restores the entry error state
--EXTENSIONS--
fastjson
--FILE--
<?php

final class PoisonOnCloseStream {
    public $context;
    public static string $contents = '{"ok":true}';
    public static string $poisonAt = 'close';
    private int $offset = 0;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        return true;
    }

    public function stream_read(int $count): string {
        if (self::$poisonAt === 'read') {
            fastjson_decode('bad');
        }
        $chunk = substr(self::$contents, $this->offset, $count);
        $this->offset += strlen($chunk);
        return $chunk;
    }

    public function stream_eof(): bool {
        return $this->offset >= strlen(self::$contents);
    }

    public function stream_close(): void {
        if (self::$poisonAt === 'close') {
            fastjson_decode('bad');
        }
    }

    public function stream_stat(): array {
        return [];
    }
}

stream_wrapper_register('fastjsonstatefile', PoisonOnCloseStream::class);
$file = tempnam(sys_get_temp_dir(), 'fastjson-state-');
file_put_contents($file, PoisonOnCloseStream::$contents);

try {
    foreach (['file', 'data', 'clean', 'read', 'close'] as $source) {
        PoisonOnCloseStream::$poisonAt = $source;
        $path = match ($source) {
            'file' => $file,
            'data' => 'data://text/plain,' . rawurlencode(PoisonOnCloseStream::$contents),
            default => 'fastjsonstatefile://valid',
        };
        foreach ([false, true] as $seedError) {
            foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
                fastjson_decode($seedError ? "{\n  \"seed\": bad\n}" : '0');
                $before = fastjson_last_error_info();
                if ($seedError && ($before['pos'] <= 0 || $before['line'] <= 1 || $before['col'] <= 1)) {
                    throw new RuntimeException('seed did not carry error location');
                }
                $result = fastjson_file_decode($path, true, 512, $flags);
                $after = fastjson_last_error_info();
                if ($result !== ['ok' => true]) {
                    throw new RuntimeException("$source returned the wrong value");
                }
                if ($flags === JSON_THROW_ON_ERROR) {
                    if ($after !== $before) {
                        throw new RuntimeException("$source did not restore entry error state");
                    }
                } else {
                    fastjson_decode('0');
                    if ($after !== fastjson_last_error_info()) {
                        throw new RuntimeException("$source did not clear error state");
                    }
                }
            }
        }
        echo "$source ok\n";
    }
} finally {
    unlink($file);
    stream_wrapper_unregister('fastjsonstatefile');
}
?>
--EXPECT--
file ok
data ok
clean ok
read ok
close ok
