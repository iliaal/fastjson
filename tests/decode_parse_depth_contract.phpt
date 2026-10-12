--TEST--
fastjson decode and pointer depth applies after full document parsing
--EXTENSIONS--
fastjson
json
--FILE--
<?php
$nested = str_repeat('[', 128) . '0' . str_repeat(']', 128);
$malformed = str_repeat('[', 128) . 'invalid';

foreach ([$nested, $malformed, '{"a":[[0]],"a":1}'] as $index => $json) {
    $expectedError = $index === 1 ? JSON_ERROR_SYNTAX : JSON_ERROR_DEPTH;
    if (fastjson_decode($json, true, 2) !== null || fastjson_last_error() !== $expectedError) {
        throw new RuntimeException('decode depth or parse precedence changed');
    }
    $path = 'data://text/plain,' . rawurlencode($json);
    if (fastjson_file_decode($path, true, 2) !== null || fastjson_last_error() !== $expectedError) {
        throw new RuntimeException('file decode depth or parse precedence changed');
    }
}
echo "decode and file depth ok\n";

$json = '{"keep":1,"discard":' . $nested . '}';
if (fastjson_pointer_get($json, '/keep', true, 2) !== 1
    || fastjson_last_error() !== JSON_ERROR_NONE
    || !fastjson_pointer_exists($json, '/keep')
    || fastjson_last_error() !== JSON_ERROR_NONE) {
    throw new RuntimeException('unselected nesting affected pointer depth');
}
if (fastjson_pointer_get($json, '/discard', true, 2) !== null
    || fastjson_last_error() !== JSON_ERROR_DEPTH) {
    throw new RuntimeException('selected subtree escaped its depth limit');
}

$json = '{"keep":1,"discard":' . $malformed . '}';
if (fastjson_pointer_get($json, '/keep', true, 2) !== null
    || fastjson_last_error() !== JSON_ERROR_SYNTAX
    || fastjson_pointer_exists($json, '/keep')
    || fastjson_last_error() !== JSON_ERROR_SYNTAX) {
    throw new RuntimeException('pointer access skipped malformed source content');
}
echo "pointer input and result depth ok\n";
?>
--EXPECT--
decode and file depth ok
pointer input and result depth ok
