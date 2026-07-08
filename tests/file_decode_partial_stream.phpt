--TEST--
fastjson_file_decode: partial stream reads fail instead of decoding truncated bytes
--EXTENSIONS--
fastjson
--FILE--
<?php

class FastjsonPartialRead
{
    public $context;
    private int $reads = 0;

    public function stream_open($path, $mode, $options, &$opened_path): bool
    {
        return true;
    }

    public function stream_read($count): string
    {
        $this->reads++;
        return $this->reads === 1 ? '1' : '';
    }

    public function stream_eof(): bool
    {
        return false;
    }

    public function stream_stat(): array
    {
        return [];
    }
}

stream_wrapper_register('fjpartial', FastjsonPartialRead::class);

var_dump(fastjson_file_decode('fjpartial://x'));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
echo fastjson_last_error_msg(), "\n";
?>
--EXPECT--
NULL
bool(true)
Failed to read file
