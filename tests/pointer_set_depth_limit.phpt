--TEST--
fastjson_pointer_set: missing-parent suffix is bounded by $depth
--EXTENSIONS--
fastjson
--FILE--
<?php

$tooDeep = str_repeat('/x', 600);
var_dump(fastjson_pointer_set('{}', $tooDeep, 1, 512));
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);
var_dump(fastjson_last_error_msg());

try {
    fastjson_pointer_set('{}', $tooDeep, 1, 512, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (JsonException $e) {
    echo "threw: ", $e->getCode(), " ", $e->getMessage(), "\n";
}

$deepTarget = str_repeat('{"a":', 1100) . '1' . str_repeat('}', 1100);
var_dump(fastjson_pointer_set($deepTarget, '/x', 1, 100000));
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);

echo fastjson_pointer_set('{}', '/a/b/c', 1, 10), "\n";
?>
--EXPECT--
bool(false)
bool(true)
string(28) "Maximum stack depth exceeded"
threw: 1 Maximum stack depth exceeded
bool(false)
bool(true)
{"a":{"b":{"c":1}}}
