--TEST--
fastjson_decode: INVALID_UTF8 flags do not relax the raw control-char check
--EXTENSIONS--
fastjson
--FILE--
<?php

/* yyjson's ALLOW_INVALID_UNICODE (enabled for JSON_INVALID_UTF8_IGNORE /
 * SUBSTITUTE to tolerate malformed UTF-8) also makes it accept raw
 * control characters inside strings. ext/json rejects those regardless
 * of its INVALID_UTF8 flags, so fastjson re-imposes the strict check.
 * The error stays consistent with the no-flag path (yyjson's own
 * "unexpected control character in string"). */

foreach ([0, JSON_INVALID_UTF8_IGNORE, JSON_INVALID_UTF8_SUBSTITUTE] as $flag) {
    $r = fastjson_decode("[\"a\x01b\"]", true, 512, $flag);
    var_dump($r);
    echo fastjson_last_error_msg(), "\n";
}

// A control char in an object key is rejected too.
var_dump(fastjson_decode("{\"k\x1fey\":1}", true, 512, JSON_INVALID_UTF8_IGNORE));

// Escaped control forms remain valid -- the scan only flags literal bytes.
var_dump(fastjson_decode("[\"a\\nb\\tc\"]", true, 512, JSON_INVALID_UTF8_IGNORE));

// Genuine invalid UTF-8 is still tolerated (stripped) under IGNORE.
var_dump(fastjson_decode("[\"a\xffb\"]", true, 512, JSON_INVALID_UTF8_IGNORE));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

// fastjson_validate() enforces the same strict control-char check.
var_dump(fastjson_validate("[\"a\x01b\"]", 512, JSON_INVALID_UTF8_IGNORE));
echo fastjson_last_error_msg(), "\n";

// RELAXED comments may contain quotes and control-ish bytes without being
// misread as string content (the check lives in yyjson's string reader,
// which comments never reach); a control char in a real string is still
// rejected under RELAXED.
var_dump(fastjson_decode("// a \" b\n[1]", true, 512, FASTJSON_DECODE_RELAXED | JSON_INVALID_UTF8_IGNORE));
var_dump(fastjson_decode("[\"a\x01b\"]", true, 512, FASTJSON_DECODE_RELAXED | JSON_INVALID_UTF8_IGNORE));
?>
--EXPECT--
NULL
unexpected control character in string
NULL
unexpected control character in string
NULL
unexpected control character in string
NULL
array(1) {
  [0]=>
  string(5) "a
b	c"
}
array(1) {
  [0]=>
  string(2) "ab"
}
bool(true)
bool(false)
unexpected control character in string
array(1) {
  [0]=>
  int(1)
}
NULL
