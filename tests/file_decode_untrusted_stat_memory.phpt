--TEST--
fastjson_file_decode: userspace stream stat size does not drive preallocation
--EXTENSIONS--
fastjson
--INI--
memory_limit=-1
--FILE--
<?php

class FastjsonOversizedStat
{
    public $context;
    private int $offset = 0;
    private const DATA = '{"ok":true}';

    public function stream_open($path, $mode, $options, &$openedPath): bool
    {
        return true;
    }

    public function stream_read($count): string
    {
        $chunk = substr(self::DATA, $this->offset, $count);
        $this->offset += strlen($chunk);
        return $chunk;
    }

    public function stream_eof(): bool
    {
        return $this->offset >= strlen(self::DATA);
    }

    public function stream_stat(): array
    {
        return ['size' => 64 * 1024 * 1024];
    }
}

stream_wrapper_register('fjoversizedstat', FastjsonOversizedStat::class);

$before = memory_get_peak_usage(true);
$result = fastjson_file_decode('fjoversizedstat://input', true);
$extraPeak = memory_get_peak_usage(true) - $before;

var_dump($result);
var_dump($extraPeak < 4 * 1024 * 1024);
?>
--EXPECT--
array(1) {
  ["ok"]=>
  bool(true)
}
bool(true)
