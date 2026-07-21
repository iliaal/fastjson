--TEST--
fastjson_pointer_set: preserve untouched numeric lexemes and replacement negative zero
--EXTENSIONS--
fastjson
--FILE--
<?php

$cases = [
    '{"n":18446744073709551616,"x":1}',
    '{"n":-9223372036854775809,"x":1}',
    '{"n":-0,"x":1}',
    '{"n":1.2300e+02,"x":1}',
];

foreach ($cases as $json) {
    echo fastjson_pointer_set($json, '/x', 2), "\n";
}

echo fastjson_pointer_set('{}', '/x', -0.0), "\n";
echo fastjson_pointer_set('{}', '/x', ['z' => -0.0]), "\n";
echo fastjson_pointer_set('{}', '/x', ['z' => -0.0], 512, JSON_PRETTY_PRINT), "\n";

var_dump(fastjson_pointer_set('{"n":1e309,"x":1}', '/x', 2));
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);
?>
--EXPECT--
{"n":18446744073709551616,"x":2}
{"n":-9223372036854775809,"x":2}
{"n":-0,"x":2}
{"n":1.2300e+02,"x":2}
{"x":-0}
{"x":{"z":-0}}
{
    "x": {
        "z": -0
    }
}
bool(false)
bool(true)
