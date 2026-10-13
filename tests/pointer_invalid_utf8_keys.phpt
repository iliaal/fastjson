--TEST--
fastjson pointer reads match decode-normalized UTF-8 keys
--EXTENSIONS--
fastjson
--FILE--
<?php

$ignore = JSON_INVALID_UTF8_IGNORE;
$substitute = JSON_INVALID_UTF8_SUBSTITUTE;
$cases = [
    ['ignore', "{\"adm\xFFin\":7}", 'admin', '/admin', $ignore],
    ['empty', "{\"\xFF\":7}", '', '/', $ignore],
    ['escapes', "{\"a\xFF/b~c\":7}", 'a/b~c', '/a~1b~0c', $ignore],
    ['nul', "{\"a\xFF\\u0000b\":7}", "a\0b", "/a\0b", $ignore],
    ['unicode', "{\"\u{00E9}\xFF\u{1F600}\":7}", "\u{00E9}\u{1F600}", "/\u{00E9}\u{1F600}", $ignore],
    ['substitute', "{\"adm\xFFin\":7}", "adm\u{FFFD}in", "/adm\u{FFFD}in", $substitute],
    ['subparts', "{\"a\xF0\x80\x80b\":7}", "a\u{FFFD}\u{FFFD}\u{FFFD}b", "/a\u{FFFD}\u{FFFD}\u{FFFD}b", $substitute],
    ['both', "{\"adm\xFFin\":7}", "adm\u{FFFD}in", "/adm\u{FFFD}in", $ignore | $substitute],
];

foreach ($cases as [$name, $json, $key, $pointer, $flags]) {
    $decoded = fastjson_decode($json, true, 512, $flags);
    $native = json_decode($json, true, 512, $flags);
    $value = fastjson_pointer_get($json, $pointer, true, 512, $flags);
    $getError = fastjson_last_error();
    $exists = fastjson_pointer_exists($json, $pointer, $flags);
    $existsError = fastjson_last_error();
    echo $name, ': ';
    var_dump($decoded === $native && $decoded[$key] === 7
        && $value === $decoded[$key] && $getError === JSON_ERROR_NONE
        && $exists && $existsError === JSON_ERROR_NONE);
}

$nested = "{\"acc\xFFount\":[{\"ro\xFFle\":7}]}";
var_dump(fastjson_pointer_get($nested, '/account/0/role', true, 512, $ignore));
var_dump(fastjson_pointer_exists($nested, '/account/0/role', $ignore));

$json = "{\"adm\xFFin\":7}";
var_dump(fastjson_pointer_get($json, "/adm\xFFin", true, 512, $ignore));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);
var_dump(fastjson_pointer_exists($json, "/adm\xFFin", $ignore));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

var_dump(fastjson_pointer_get($json, '/admin'));
var_dump(fastjson_last_error() === JSON_ERROR_UTF8);
var_dump(fastjson_pointer_exists($json, '/admin'));
var_dump(fastjson_last_error() === JSON_ERROR_UTF8);
foreach ([0, $ignore, $substitute, $ignore | $substitute] as $flags) {
    var_dump(fastjson_pointer_set($json, '/admin', 8, 512, $flags));
    var_dump(fastjson_last_error() === JSON_ERROR_UTF8);
}
?>
--EXPECT--
ignore: bool(true)
empty: bool(true)
escapes: bool(true)
nul: bool(true)
unicode: bool(true)
substitute: bool(true)
subparts: bool(true)
both: bool(true)
int(7)
bool(true)
NULL
bool(true)
bool(false)
bool(true)
NULL
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
