--TEST--
Partial output takes precedence over throwing and publishes the encode error
--EXTENSIONS--
fastjson
--FILE--
<?php
$path = __DIR__ . '/encode_partial_throw_error_state.tmp';
$flags = JSON_PARTIAL_OUTPUT_ON_ERROR | JSON_THROW_ON_ERROR;
foreach ([false, true] as $seedError) {
    foreach ([INF, "\xff", [1, 2]] as $value) {
        json_decode($seedError ? '{' : 'null');
        $expected = json_encode($value, $flags);
        $error = json_last_error();
        fastjson_decode($seedError ? '{' : 'null');
        var_dump(fastjson_encode($value, $flags) === $expected);
        var_dump(fastjson_last_error() === $error);
        fastjson_decode($seedError ? '{' : 'null');
        var_dump(fastjson_file_encode($path, $value, $flags));
        var_dump(file_get_contents($path) === $expected);
        var_dump(fastjson_last_error() === $error);
    }
}
unlink($path);
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/encode_partial_throw_error_state.tmp');
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
