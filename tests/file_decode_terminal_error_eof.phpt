--TEST--
fastjson_file_decode: terminal wrapper read error is not hidden by EOF
--EXTENSIONS--
fastjson
--FILE--
<?php

class FastjsonTerminalReadError
{
    public $context;
    private int $reads = 0;

    public function stream_open($path, $mode, $options, &$openedPath): bool
    {
        return true;
    }

    public function stream_read($count): string|false
    {
        $this->reads++;
        return $this->reads === 1 ? '{"ok":true}' : false;
    }

    public function stream_eof(): bool
    {
        return $this->reads >= 2;
    }

    public function stream_stat(): array
    {
        return [];
    }
}

stream_wrapper_register('fjterminalerror', FastjsonTerminalReadError::class);

var_dump(fastjson_file_decode('fjterminalerror://input', true));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
echo fastjson_last_error_msg(), "\n";
?>
--EXPECT--
NULL
bool(true)
Failed to read file
