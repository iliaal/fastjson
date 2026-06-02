--TEST--
fastjson_file_encode: I/O failure and encode failure; THROW scoped to encode errors
--EXTENSIONS--
fastjson
--FILE--
<?php

// I/O failure: target dir does not exist. Silent (no warning) => false.
$bad = '/nonexistent-fastjson-dir-' . getmypid() . '/x.json';
var_dump(fastjson_file_encode($bad, ['a' => 1]));
var_dump(fastjson_last_error() !== JSON_ERROR_NONE);

echo "---\n";

// Encode failure (INF, no PARTIAL_OUTPUT): false, file is never opened/written.
$file = sys_get_temp_dir() . '/fastjson_file_encode_errors.json';
@unlink($file);
var_dump(fastjson_file_encode($file, INF));
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);
var_dump(file_exists($file));

echo "---\n";

// Encode failure under JSON_THROW_ON_ERROR throws.
try {
    fastjson_file_encode($file, INF, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (\JsonException $e) {
    echo "threw: ", get_class($e), "\n";
}

echo "---\n";

// I/O failure does NOT throw, even under JSON_THROW_ON_ERROR (it's not a
// JSON error); the value encodes fine, only the write fails.
var_dump(fastjson_file_encode($bad, ['a' => 1], JSON_THROW_ON_ERROR));
?>
--EXPECT--
bool(false)
bool(true)
---
bool(false)
bool(true)
bool(false)
---
threw: JsonException
---
bool(false)
