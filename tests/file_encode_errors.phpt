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

echo "--- existing destination ---\n";
file_put_contents($file, 'keep these bytes');
var_dump(fastjson_file_encode($file, INF));
var_dump(file_get_contents($file) === 'keep these bytes');
try {
    fastjson_file_encode($file, INF, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (\JsonException $e) {
    echo "threw: ", get_class($e), "\n";
}
var_dump(file_get_contents($file) === 'keep these bytes');

// Partial output produces bytes even with an error and may replace the file.
foreach ([JSON_PARTIAL_OUTPUT_ON_ERROR,
          JSON_PARTIAL_OUTPUT_ON_ERROR | JSON_THROW_ON_ERROR] as $flags) {
    file_put_contents($file, 'keep these bytes');
    var_dump(fastjson_file_encode($file, [INF], $flags));
    var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);
    var_dump(file_get_contents($file) === '[0]');
}
unlink($file);
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
--- existing destination ---
bool(false)
bool(true)
threw: JsonException
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
