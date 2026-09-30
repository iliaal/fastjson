--TEST--
fastjson_validate reports the same error message, code, and position as fastjson_decode
--EXTENSIONS--
fastjson
--FILE--
<?php

$cases = [
    "[\"\xFF\"]",
    "[\"\\n\xFF\"]",
    "{\"\xFF\":1}",
    "{\"\\t\xFF\":1}",
    "\"\xFF\"",
    '["\ud800"]',
    '["\ud800x"]',
    '["\ud800\u12"]',
    '["\ud800\u0041"]',
    '["\udc00"]',
    '{"\udc00":1}',
    '{"\ud800\u0041":1}',
    '["\x"]',
    '["\u12G4"]',
    '{1:2}',
    '{"a" 1}',
    '[1,]',
    '{"a":1,}',
    '[1 2]',
    '{"a":1 "b":2}',
    '[tru]',
    '[1] x',
];

foreach ($cases as $input) {
    $ok = fastjson_validate($input);
    $v = [fastjson_last_error(), fastjson_last_error_msg(), fastjson_last_error_pos()];
    fastjson_decode($input);
    $d = [fastjson_last_error(), fastjson_last_error_msg(), fastjson_last_error_pos()];
    if ($v !== $d) {
        echo "MISMATCH ", bin2hex($input), ": ", json_encode($v), " vs ", json_encode($d), "\n";
    }
    printf("%s pos=%d %s\n", $ok ? 'ok' : 'invalid', $v[2], $v[1]);
}
?>
--EXPECT--
invalid pos=2 invalid UTF-8 encoding in string
invalid pos=4 invalid utf-8 encoding in string
invalid pos=2 invalid UTF-8 encoding in string
invalid pos=4 invalid utf-8 encoding in string
invalid pos=1 invalid UTF-8 encoding in string
invalid pos=2 no low surrogate in string
invalid pos=2 no low surrogate in string
invalid pos=2 invalid escape in string
invalid pos=2 invalid low surrogate in string
invalid pos=2 invalid high surrogate in string
invalid pos=2 invalid high surrogate in string
invalid pos=2 invalid low surrogate in string
invalid pos=2 invalid escaped sequence in string
invalid pos=2 invalid escaped sequence in string
invalid pos=1 unexpected character, expected a string key
invalid pos=5 unexpected character, expected ':' after key
invalid pos=2 trailing comma is not allowed
invalid pos=6 trailing comma is not allowed
invalid pos=3 unexpected character, expected ',' or ']'
invalid pos=7 unexpected character, expected ',' or '}'
invalid pos=1 invalid literal, expected 'true'
invalid pos=4 unexpected content after document
