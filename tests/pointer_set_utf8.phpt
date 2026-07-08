--TEST--
fastjson_pointer_set: UTF-8 flags sanitize the replacement value
--EXTENSIONS--
fastjson
--FILE--
<?php

/* The $flags UTF-8 bits (JSON_INVALID_UTF8_IGNORE / SUBSTITUTE) apply to
 * the replacement value the same way fastjson_encode() honors them, so an
 * invalid string in $value no longer fails the whole set. Output-
 * formatting flags still drive the write; UTF-8 bits are not written into
 * the document. A base document containing invalid UTF-8 is rejected at
 * parse with a clean JSON_ERROR_UTF8 rather than an opaque write failure,
 * because pointer_set serializes the document back through yyjson. */

$badval = "a\xFFb";

// IGNORE strips the invalid byte from the replacement.
var_dump(fastjson_pointer_set('{}', '/bad', $badval, 512, JSON_INVALID_UTF8_IGNORE));
var_dump(fastjson_last_error());

// SUBSTITUTE replaces it with U+FFFD.
var_dump(fastjson_pointer_set('{}', '/bad', $badval, 512, JSON_INVALID_UTF8_SUBSTITUTE));

// No UTF-8 flag: an invalid replacement is a hard UTF-8 error.
var_dump(fastjson_pointer_set('{}', '/bad', $badval));
var_dump(fastjson_last_error());

// Invalid UTF-8 in the base document is rejected at parse (error 5),
// even with IGNORE set (the writer cannot emit those bytes).
$doc = "{\"bad\":\"a\xFFb\",\"x\":1}";
var_dump(fastjson_pointer_set($doc, '/x', 2, 512, JSON_INVALID_UTF8_IGNORE));
var_dump(fastjson_last_error());

// Output-formatting flags still take effect on the write.
var_dump(fastjson_pointer_set('{"p":"x"}', '/p', 'a/b', 512, JSON_UNESCAPED_SLASHES));
var_dump(fastjson_pointer_set('{"p":"x"}', '/p', 'a/b', 512, 0));
?>
--EXPECT--
string(12) "{"bad":"ab"}"
int(0)
string(18) "{"bad":"a\ufffdb"}"
bool(false)
int(5)
bool(false)
int(5)
string(11) "{"p":"a/b"}"
string(12) "{"p":"a\/b"}"
