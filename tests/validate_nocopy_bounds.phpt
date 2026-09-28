--TEST--
validate at eof matches decode errors and leaves the input unchanged
--EXTENSIONS--
fastjson
--FILE--
<?php

$cases = [
    'x',
    "\"\xC2",
    '-',
    '1.',
    '1e',
    '1e+',
    '[' . str_repeat('1', 32),
    str_repeat('1', 40),
    '1e309',
    '[1e309, n]',
    '[1e309, in',
    '[1e309, n',
];

foreach ($cases as $input) {
    $snapshot = substr($input, 0);
    $ok = fastjson_validate($input);
    $vmsg = fastjson_last_error_msg();
    fastjson_decode($snapshot);
    if (fastjson_last_error_msg() !== $vmsg) {
        echo "MISMATCH\n";
    }
    var_dump($ok, $input === $snapshot, $vmsg);
}
?>
--EXPECT--
bool(false)
bool(true)
string(43) "unexpected character, expected a JSON value"
bool(false)
bool(true)
string(22) "unexpected end of data"
bool(false)
bool(true)
string(22) "unexpected end of data"
bool(false)
bool(true)
string(22) "unexpected end of data"
bool(false)
bool(true)
string(22) "unexpected end of data"
bool(false)
bool(true)
string(22) "unexpected end of data"
bool(false)
bool(true)
string(22) "unexpected end of data"
bool(true)
bool(true)
string(8) "No error"
bool(true)
bool(true)
string(8) "No error"
bool(false)
bool(true)
string(32) "invalid literal, expected 'null'"
bool(false)
bool(true)
string(22) "unexpected end of data"
bool(false)
bool(true)
string(22) "unexpected end of data"
