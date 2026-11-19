--TEST--
fastjson_decode() with JSON_INVALID_UTF8_IGNORE/SUBSTITUTE stays linear in the number of invalid bytes
--EXTENSIONS--
fastjson
--FILE--
<?php
/* Every invalid byte used to restart the vector UTF-8 scan over the rest
 * of the string, so a string of n invalid bytes took O(n^2) time: 400 KB
 * ran for minutes. */
$n = 1 << 20;
$json = '"' . str_repeat("\xff", $n) . '"';
$t = hrtime(true);
$ignored = fastjson_decode($json, false, 512, JSON_INVALID_UTF8_IGNORE);
$substituted = fastjson_decode($json, false, 512, JSON_INVALID_UTF8_SUBSTITUTE);
$seconds = (hrtime(true) - $t) / 1e9;
var_dump($ignored === '');
var_dump($substituted === str_repeat("\u{fffd}", $n));
echo $seconds < 20 ? "linear\n" : "too slow: {$seconds}s\n";

/* Invalid bytes after valid multi-byte runs: the scan must hand back the
 * valid prefix, and the scalar code must find the same bytes ext/json does. */
foreach ([
    str_repeat(str_repeat("\xc3\xa9", 20) . "\xff", 2000),
    str_repeat(str_repeat("\xe6\x97\xa5", 7) . "\xe6\x97", 3000),
    str_repeat("\xf0\x9f\x98\x80" . str_repeat("a", 9) . "\x80\xc3", 3000),
] as $i => $s) {
    $json = '"' . $s . '"';
    foreach ([JSON_INVALID_UTF8_IGNORE, JSON_INVALID_UTF8_SUBSTITUTE] as $flag) {
        echo $i, ' ', $flag === JSON_INVALID_UTF8_IGNORE ? 'ignore' : 'substitute', ': ',
            fastjson_decode($json, false, 512, $flag) === json_decode($json, false, 512, $flag)
                ? 'same' : 'differs', "\n";
    }
    var_dump(fastjson_decode($json), json_decode($json));
    echo fastjson_last_error() === json_last_error() ? "same error\n" : "error differs\n";
}

$json = '"' . str_repeat("\xc3\xa9", 100) . "\xff" . '"';
var_dump(fastjson_validate($json), fastjson_last_error_pos());
?>
--EXPECT--
bool(true)
bool(true)
linear
0 ignore: same
0 substitute: same
NULL
NULL
same error
1 ignore: same
1 substitute: same
NULL
NULL
same error
2 ignore: same
2 substitute: same
NULL
NULL
same error
bool(false)
int(201)
