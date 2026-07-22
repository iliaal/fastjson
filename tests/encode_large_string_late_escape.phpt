--TEST--
fastjson_encode: large strings with late escapes and UTF-8 match ext/json
--EXTENSIONS--
fastjson
--FILE--
<?php

$prefix = str_repeat('a', 256 * 1024);
$cases = [
    [$prefix . '"', 0],
    [$prefix . '\\', 0],
    [$prefix . "\n", 0],
    [$prefix . '/', 0],
    [$prefix . '/', JSON_UNESCAPED_SLASHES],
    [$prefix . "é", 0],
    [$prefix . "é", JSON_UNESCAPED_UNICODE],
];

foreach ($cases as [$value, $flags]) {
    var_dump(fastjson_encode($value, $flags) === json_encode($value, $flags));
}

$invalid = $prefix . "\xFF";
foreach ([0, JSON_INVALID_UTF8_IGNORE, JSON_INVALID_UTF8_SUBSTITUTE] as $flags) {
    $native = json_encode($invalid, $flags);
    $nativeError = json_last_error();
    $actual = fastjson_encode($invalid, $flags);
    var_dump($actual === $native);
    var_dump(fastjson_last_error() === $nativeError);
}
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
