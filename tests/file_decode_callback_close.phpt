--TEST--
fastjson_file_decode keeps its stream alive through callbacks that call pclose or fclose
--EXTENSIONS--
fastjson
--FILE--
<?php
final class DecodeCallbackCloseStream {
    public $context;
    public static string $closer;
    public static string $stage;
    public static string $outcome;
    public static int $reads;
    public static int $closes;
    public static bool $active;
    public static bool $searching;
    public static bool $closedDuringCallback;
    public static bool $attempted;
    public static mixed $resource;

    public function stream_open(string $path, string $mode, int $options, ?string &$openedPath): bool {
        self::$reads = 0;
        self::$closes = 0;
        self::$active = false;
        self::$searching = false;
        self::$closedDuringCallback = false;
        self::$attempted = false;
        self::$resource = null;
        return true;
    }

    private function closeFromCallback(): void {
        if (self::$attempted || self::$searching) {
            return;
        }
        self::$searching = true;
        try {
            foreach (get_resources('stream') as $stream) {
                if ((stream_get_meta_data($stream)['wrapper_data'] ?? null) === $this) {
                    self::$resource = $stream;
                    self::$attempted = true;
                    self::$active = true;
                    try {
                        $close = self::$closer;
                        @$close($stream);
                    } finally {
                        self::$active = false;
                    }
                    if (self::$outcome === 'throw') {
                        throw new RuntimeException('callback-close');
                    }
                    return;
                }
            }
        } finally {
            self::$searching = false;
        }
        throw new RuntimeException('stream not found');
    }

    public function stream_read(int $count): string|false {
        if (self::$stage === 'read' && self::$reads === 0) {
            $this->closeFromCallback();
            if (self::$outcome === 'fail') {
                self::$reads++;
                return false;
            }
        }
        return self::$reads++ === 0 ? '{"ok":true}' : '';
    }

    public function stream_eof(): bool {
        if (self::$stage === 'eof' && self::$reads > 0) {
            $this->closeFromCallback();
            if (self::$outcome === 'fail') {
                return false;
            }
        }
        return self::$reads > 1;
    }

    public function stream_stat(): array { return []; }

    public function stream_close(): void {
        self::$closes++;
        self::$closedDuringCallback = self::$closedDuringCallback || self::$active;
    }
}

stream_wrapper_register('fjdecodeclose', DecodeCallbackCloseStream::class);
foreach (['pclose', 'fclose'] as $closer) {
    foreach (['read', 'eof'] as $stage) {
        foreach (['success', 'fail', 'throw'] as $outcome) {
            foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
                DecodeCallbackCloseStream::$closer = $closer;
                DecodeCallbackCloseStream::$stage = $stage;
                DecodeCallbackCloseStream::$outcome = $outcome;
                fastjson_decode('{');
                $saved = fastjson_last_error_info();
                $thrown = null;
                $result = 'unset';
                try {
                    $result = fastjson_file_decode('fjdecodeclose://src', true, 512, $flags);
                } catch (RuntimeException $exception) {
                    $thrown = $exception->getMessage();
                }
                $valid = DecodeCallbackCloseStream::$attempted
                    && DecodeCallbackCloseStream::$closes <= 1
                    && !DecodeCallbackCloseStream::$closedDuringCallback
                    && !is_resource(DecodeCallbackCloseStream::$resource);
                if ($outcome === 'throw') {
                    $valid = $valid && $thrown === 'callback-close' && $result === 'unset'
                        && DecodeCallbackCloseStream::$closes === 0
                        && ($flags ? fastjson_last_error_info() === $saved
                            : fastjson_last_error() === JSON_ERROR_NONE);
                } elseif ($outcome === 'success') {
                    $valid = $valid && $thrown === null && $result === ['ok' => true]
                        && DecodeCallbackCloseStream::$closes === 1
                        && ($flags ? fastjson_last_error_info() === $saved
                            : fastjson_last_error() === JSON_ERROR_NONE);
                } else {
                    $valid = $valid && $thrown === null && $result === null
                        && DecodeCallbackCloseStream::$closes === 1
                        && fastjson_last_error() === JSON_ERROR_SYNTAX
                        && fastjson_last_error_msg() === 'Failed to read file';
                }
                echo "$closer $stage $outcome ", $flags ? 'throw' : 'normal', ': ',
                    $valid ? 'ok' : 'FAIL ' . json_encode([
                        'result' => $result, 'thrown' => $thrown,
                        'closes' => DecodeCallbackCloseStream::$closes,
                        'during' => DecodeCallbackCloseStream::$closedDuringCallback,
                        'error' => fastjson_last_error_msg(),
                    ]), "\n";
            }
        }
    }
}
stream_wrapper_unregister('fjdecodeclose');
?>
--EXPECT--
pclose read success normal: ok
pclose read success throw: ok
pclose read fail normal: ok
pclose read fail throw: ok
pclose read throw normal: ok
pclose read throw throw: ok
pclose eof success normal: ok
pclose eof success throw: ok
pclose eof fail normal: ok
pclose eof fail throw: ok
pclose eof throw normal: ok
pclose eof throw throw: ok
fclose read success normal: ok
fclose read success throw: ok
fclose read fail normal: ok
fclose read fail throw: ok
fclose read throw normal: ok
fclose read throw throw: ok
fclose eof success normal: ok
fclose eof success throw: ok
fclose eof fail normal: ok
fclose eof fail throw: ok
fclose eof throw normal: ok
fclose eof throw throw: ok
