--TEST--
fastjson_file_decode/encode: honor the default stream context (like file_get/put_contents)
--EXTENSIONS--
fastjson
--FILE--
<?php

class CtxProbe
{
    /** @var resource set by the streams layer to the context in effect */
    public $context;
    public static $seen = 'UNSET';

    private string $buf = '{"ok":true}';
    private int $pos = 0;

    public function stream_open($path, $mode, $options, &$opened_path): bool
    {
        // Record the token visible via whatever context the opener passed.
        // file_get_contents/file_put_contents pass the request default
        // context; a NULL context would expose no options.
        $opts = $this->context ? stream_context_get_options($this->context) : [];
        self::$seen = $opts['ctxprobe']['token'] ?? 'NONE';
        return true;
    }
    public function stream_read($n): string
    {
        $r = substr($this->buf, $this->pos, $n);
        $this->pos += strlen($r);
        return $r;
    }
    public function stream_write($data): int { return strlen($data); }
    public function stream_eof(): bool { return $this->pos >= strlen($this->buf); }
    public function stream_stat() { return []; }
    public function stream_close(): void {}
    public function stream_set_option($o, $a, $b): bool { return false; }
}

stream_wrapper_register('ctxprobe', CtxProbe::class);
stream_context_set_default(['ctxprobe' => ['token' => 'MARK']]);

// Read path.
CtxProbe::$seen = 'UNSET';
$r = fastjson_file_decode('ctxprobe://x', true);
var_dump($r);
var_dump(CtxProbe::$seen);   // MARK => default context was passed through

echo "---\n";

// Write path.
CtxProbe::$seen = 'UNSET';
var_dump(fastjson_file_encode('ctxprobe://y', ['a' => 1]));
var_dump(CtxProbe::$seen);   // MARK => default context was passed through
?>
--EXPECT--
array(1) {
  ["ok"]=>
  bool(true)
}
string(4) "MARK"
---
bool(true)
string(4) "MARK"
