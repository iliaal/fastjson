--TEST--
fastjson_file_decode releases a guarded stream when a read callback bails out
--EXTENSIONS--
fastjson
--INI--
display_errors=0
log_errors=0
--FILE--
<?php
final class DecodeBailoutStream {
    public $context;
    public static int $closes = 0;
    public static mixed $resource = null;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        return true;
    }

    public function stream_read(int $count): string {
        foreach (get_resources('stream') as $stream) {
            if ((stream_get_meta_data($stream)['wrapper_data'] ?? null) === $this) {
                self::$resource = $stream;
                pclose($stream);
                trigger_error('stop', E_USER_ERROR);
            }
        }
        return '';
    }

    public function stream_eof(): bool { return false; }
    public function stream_stat(): array { return []; }
    public function stream_close(): void { self::$closes++; }
}

stream_wrapper_register('fjdecodebailout', DecodeBailoutStream::class);
register_shutdown_function(static function (): void {
    echo 'closed=', DecodeBailoutStream::$closes,
         ', live=', is_resource(DecodeBailoutStream::$resource) ? 'yes' : 'no', "\n";
});
fastjson_file_decode('fjdecodebailout://src');
?>
--EXPECT--
closed=1, live=no
