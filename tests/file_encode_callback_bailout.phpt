--TEST--
fastjson_file_encode releases a guarded stream when a write callback bails out
--EXTENSIONS--
fastjson
--INI--
display_errors=0
log_errors=0
--FILE--
<?php
final class BailoutStream {
    public $context;
    public static int $closes = 0;
    public static mixed $resource = null;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        return true;
    }

    public function stream_write(string $data): int {
        foreach (get_resources('stream') as $stream) {
            if ((stream_get_meta_data($stream)['wrapper_data'] ?? null) === $this) {
                self::$resource = $stream;
                pclose($stream);
                trigger_error('stop', E_USER_ERROR);
            }
        }
        return strlen($data);
    }

    public function stream_flush(): bool { return true; }
    public function stream_close(): void { self::$closes++; }
}

stream_wrapper_register('fjbailout', BailoutStream::class);
register_shutdown_function(static function (): void {
    echo 'closed=', BailoutStream::$closes,
         ', live=', is_resource(BailoutStream::$resource) ? 'yes' : 'no', "\n";
});
fastjson_file_encode('fjbailout://sink', ['ok' => true]);
?>
--EXPECT--
closed=1, live=no
