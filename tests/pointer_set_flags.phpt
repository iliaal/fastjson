--TEST--
fastjson_pointer_set: encode flags apply to replacement values and output strings
--EXTENSIONS--
fastjson
--FILE--
<?php

echo fastjson_pointer_set('{}', '/x', [1, 2], 512, JSON_FORCE_OBJECT), "\n";
echo fastjson_pointer_set('{}', '/x', '123', 512, JSON_NUMERIC_CHECK), "\n";
echo fastjson_pointer_set('{}', '/x', 1.0, 512, JSON_PRESERVE_ZERO_FRACTION), "\n";
echo fastjson_pointer_set('{}', '/x', INF, 512, JSON_PARTIAL_OUTPUT_ON_ERROR), "\n";
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);

$hexFlags = JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT;
echo fastjson_pointer_set('{"a":"<>&\'\""}', '/b', '<>&\'"', 512, $hexFlags), "\n";
?>
--EXPECT--
{"x":{"0":1,"1":2}}
{"x":123}
{"x":1.0}
{"x":0}
bool(true)
{"a":"\u003C\u003E\u0026\u0027\u0022","b":"\u003C\u003E\u0026\u0027\u0022"}
