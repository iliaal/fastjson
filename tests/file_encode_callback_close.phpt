--TEST--
fastjson_file_encode keeps its stream alive through callbacks that call pclose
--EXTENSIONS--
fastjson
--FILE--
<?php
final class CallbackCloseStream {
    public $context;
    public static string $stage;
    public static string $outcome;
    public static string $data;
    public static int $closes;
    public static bool $active;
    public static bool $closedDuringCallback;
    public static bool $attempted;
    public static mixed $resource;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        self::$data = '';
        self::$closes = 0;
        self::$active = false;
        self::$closedDuringCallback = false;
        self::$attempted = false;
        self::$resource = null;
        return true;
    }

    private function closeFromCallback(): void {
        if (self::$attempted) {
            return;
        }
        foreach (get_resources('stream') as $stream) {
            if ((stream_get_meta_data($stream)['wrapper_data'] ?? null) === $this) {
                self::$resource = $stream;
                self::$attempted = true;
                self::$active = true;
                try {
                    pclose($stream);
                } finally {
                    self::$active = false;
                }
                if (self::$outcome === 'throw') {
                    throw new RuntimeException('callback-close');
                }
                return;
            }
        }
        throw new RuntimeException('stream not found');
    }

    public function stream_write(string $data): int {
        if (self::$stage === 'write') {
            $this->closeFromCallback();
            if (self::$outcome === 'fail') {
                return 0;
            }
        }
        $part = substr($data, 0, 2);
        self::$data .= $part;
        return strlen($part);
    }

    public function stream_flush(): bool {
        if (self::$stage === 'flush') {
            $this->closeFromCallback();
            return self::$outcome !== 'fail';
        }
        return true;
    }

    public function stream_eof(): bool { return false; }

    public function stream_close(): void {
        self::$closes++;
        self::$closedDuringCallback = self::$closedDuringCallback || self::$active;
    }
}

stream_wrapper_register('fjcallbackclose', CallbackCloseStream::class);
foreach (['flush', 'write'] as $stage) {
    foreach (['success', 'fail', 'throw'] as $outcome) {
        foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
            CallbackCloseStream::$stage = $stage;
            CallbackCloseStream::$outcome = $outcome;
            fastjson_decode('{');
            $saved = fastjson_last_error_info();
            $thrown = null;
            try {
                $result = fastjson_file_encode('fjcallbackclose://sink', ['ok' => true], $flags);
            } catch (RuntimeException $exception) {
                $thrown = $exception->getMessage();
            }
            $valid = CallbackCloseStream::$attempted
                && CallbackCloseStream::$closes === ($outcome === 'throw' ? 0 : 1)
                && !CallbackCloseStream::$closedDuringCallback
                && !is_resource(CallbackCloseStream::$resource);
            if ($outcome === 'throw') {
                $valid = $valid && $thrown === 'callback-close';
            } else {
                $valid = $valid && $thrown === null && $result === ($outcome === 'success');
            }
            if ($outcome === 'success' || ($stage === 'flush' && $outcome === 'fail')) {
                $valid = $valid && CallbackCloseStream::$data === '{"ok":true}';
            }
            $valid = $valid && ($outcome === 'fail'
                ? fastjson_last_error() === JSON_ERROR_SYNTAX
                : ($flags ? fastjson_last_error_info() === $saved
                    : fastjson_last_error() === JSON_ERROR_NONE));
            echo "$stage $outcome ", $flags ? 'throw' : 'normal', ': ', $valid ? 'ok' : 'FAIL', "\n";
        }
    }
}
stream_wrapper_unregister('fjcallbackclose');
?>
--EXPECT--
flush success normal: ok
flush success throw: ok
flush fail normal: ok
flush fail throw: ok
flush throw normal: ok
flush throw throw: ok
write success normal: ok
write success throw: ok
write fail normal: ok
write fail throw: ok
write throw normal: ok
write throw throw: ok
