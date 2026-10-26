--TEST--
fastjson_file_decode: I/O and parse failures; THROW scoped to JSON errors only
--EXTENSIONS--
fastjson
--FILE--
<?php

$missing = sys_get_temp_dir() . '/fastjson_does_not_exist_' . getmypid() . '.json';
@unlink($missing);

// Missing file: silent (no warning), returns null, last_error set non-zero.
var_dump(fastjson_file_decode($missing));
var_dump(fastjson_last_error() !== JSON_ERROR_NONE);
echo fastjson_last_error_msg(), "\n";

echo "---\n";

// Invalid JSON in an existing file: null + parse error.
$file = sys_get_temp_dir() . '/fastjson_file_decode_errors.json';
file_put_contents($file, '{not json');
var_dump(fastjson_file_decode($file));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);

echo "---\n";

// JSON_THROW_ON_ERROR throws on a parse error.
try {
    fastjson_file_decode($file, true, 512, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (\JsonException $e) {
    echo "threw: ", get_class($e), "\n";
}

echo "---\n";

// But an I/O failure is NOT a JSON error: still returns null, never throws,
// even under JSON_THROW_ON_ERROR.
$r = fastjson_file_decode($missing, true, 512, JSON_THROW_ON_ERROR);
var_dump($r);

// Observe open attempts directly; a missing path alone cannot prove ordering.
class DepthOpenProbe {
    public $context;
    public static $opens = 0;
    public function stream_open($path, $mode, $options, &$openedPath) {
        self::$opens++;
        return false;
    }
}
stream_wrapper_register('fjdepthguard', DepthOpenProbe::class);
$invalidDepths = [0, -1];
if (PHP_INT_SIZE > 4) {
    $invalidDepths[] = 2147483648;
}
foreach ([0, JSON_THROW_ON_ERROR] as $flags) {
    foreach ($invalidDepths as $depth) {
        try {
            fastjson_file_decode('fjdepthguard://input', true, $depth, $flags);
            echo "no depth error\n";
        } catch (\ValueError $e) {
            // Expected before any stream_open callback.
        }
    }
    var_dump(DepthOpenProbe::$opens === 0);
}
stream_wrapper_unregister('fjdepthguard');
unlink($file);
?>
--EXPECT--
NULL
bool(true)
Failed to open file for reading
---
NULL
bool(true)
---
threw: JsonException
---
NULL
bool(true)
bool(true)
