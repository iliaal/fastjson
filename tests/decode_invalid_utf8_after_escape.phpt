--TEST--
fastjson_decode: invalid UTF-8 stays tolerated in strings that carry an escape
--EXTENSIONS--
fastjson
--FILE--
<?php

/* A backslash escape switches yyjson's string reader from its skip path to
 * its copy path. On the copy path a byte tolerated by ALLOW_INVALID_UTF8 is
 * copied through and the reader re-enters copy_utf8, which routes the next
 * ordinary ASCII byte to copy_escape. That arm must copy the byte and carry
 * on -- it is not a control character. Vendor patch P-004 must keep rejecting
 * genuine control bytes without capturing this case. */

$cases = [
    '"\\/' . "\xFF" . 'e"',
    '"a\\nb' . "\xFF" . 'c"',
    '"\\t' . "\xC3" . '.tail"',
    '"\\u0041' . "\xFF" . 'z"',
    '"\\\\' . "\xE0" . '.x"',
    // invalid byte last, before the closing quote (skip-path equivalent)
    '"a\\n' . "\xFF" . '"',
    // no escape at all: the skip path, already correct before the fix
    '"a' . "\xFF" . 'b"',
    // several invalid bytes interleaved with ordinary text
    '"\\n' . "\xFF" . 'a' . "\xC3" . 'b' . "\xF5" . 'c"',
];

foreach ([JSON_INVALID_UTF8_IGNORE, JSON_INVALID_UTF8_SUBSTITUTE] as $flag) {
    foreach ($cases as $json) {
        $native = json_decode($json, true, 512, $flag);
        $fast = fastjson_decode($json, true, 512, $flag);
        var_dump($fast === $native);
        var_dump($fast);
    }
}

/* Raw control characters are still rejected on the copy path. */
var_dump(fastjson_decode('"a\\nb' . "\x01" . 'c"', true, 512, JSON_INVALID_UTF8_IGNORE));
echo fastjson_last_error_msg(), "\n";

/* Without a UTF-8 handling flag the invalid byte is still an error. */
var_dump(fastjson_decode('"\\/' . "\xFF" . 'e"', true, 512, 0));
var_dump(fastjson_last_error() === JSON_ERROR_UTF8);

/* fastjson_validate() agrees with json_validate() on the same inputs. */
foreach ($cases as $json) {
    var_dump(fastjson_validate($json, 512, JSON_INVALID_UTF8_IGNORE)
        === json_validate($json, 512, JSON_INVALID_UTF8_IGNORE));
}
?>
--EXPECT--
bool(true)
string(2) "/e"
bool(true)
string(4) "a
bc"
bool(true)
string(6) "	.tail"
bool(true)
string(2) "Az"
bool(true)
string(3) "\.x"
bool(true)
string(2) "a
"
bool(true)
string(2) "ab"
bool(true)
string(4) "
abc"
bool(true)
string(5) "/�e"
bool(true)
string(7) "a
b�c"
bool(true)
string(9) "	�.tail"
bool(true)
string(5) "A�z"
bool(true)
string(6) "\�.x"
bool(true)
string(5) "a
�"
bool(true)
string(5) "a�b"
bool(true)
string(13) "
�a�b�c"
NULL
unexpected control character in string
NULL
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
