<?php
// Exercises the smart_str growth paths of a prebuilt module, so an import the
// running PHP does not export fails here: PHP loads extensions with RTLD_LAZY,
// and an unresolved function only aborts when first called.

function fail(string $msg): never
{
    fwrite(STDERR, "FAIL: $msg\n");
    exit(1);
}

if (!extension_loaded('fastjson')) {
    fail('fastjson is not loaded');
}

$value = [];
for ($i = 0; $i < 2000; $i++) {
    $value["k$i"] = str_repeat("\"\\/\u{e9}\x01", 4) . $i;
}

$json = fastjson_encode($value);
if (!is_string($json) || strlen($json) < 64 * 1024) {
    fail('fastjson_encode() did not produce the expected large document');
}
if (json_decode($json, true) !== $value) {
    fail('fastjson_encode() output does not round-trip through json_decode()');
}

$pretty = fastjson_encode($value, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
if (!is_string($pretty) || json_decode($pretty, true) !== $value) {
    fail('pretty-printed fastjson_encode() output does not round-trip');
}

if (fastjson_decode($json, true) !== $value) {
    fail('fastjson_decode() does not round-trip');
}

// data: streams are not STDIO streams, so this read goes through the
// chunked smart_str buffer rather than php_stream_copy_to_mem().
$decoded = fastjson_file_decode('data://application/json;base64,' . base64_encode($json), true);
if ($decoded !== $value) {
    fail('fastjson_file_decode() on a non-STDIO stream does not round-trip');
}

printf("ok php=%s fastjson=%s encoded_bytes=%d\n", PHP_VERSION, fastjson_version(), strlen($json));
