--TEST--
fastjson_pointer_set: an untouched non-finite number never re-emits as Infinity
--EXTENSIONS--
fastjson
--FILE--
<?php

/* fastjson's reader retries exponent-overflow numbers (1e309) with
 * ALLOW_INF_AND_NAN to match ext/json's decode-to-INF. pointer_set
 * re-emits untouched nodes verbatim, so without a guard that INF would
 * be written back as the invalid JSON token `Infinity`. The splice
 * writer now rejects non-finite reals with JSON_ERROR_INF_OR_NAN, the
 * same contract fastjson_encode() honors. */

// Editing an unrelated key must not smuggle Infinity into the output.
var_dump(fastjson_pointer_set('{"x":1e309,"y":2}', '/y', 3));
var_dump(fastjson_last_error() === JSON_ERROR_INF_OR_NAN);
echo fastjson_last_error_msg(), "\n";

// THROW_ON_ERROR surfaces it as an exception when an untouched sibling
// carries the non-finite value (editing /b re-emits /a's -1e309).
try {
    fastjson_pointer_set('{"a":-1e309,"b":2}', '/b', 5, 512, JSON_THROW_ON_ERROR);
} catch (JsonException $e) {
    echo "threw: ", $e->getMessage(), "\n";
}

// A document with only finite numbers still round-trips normally.
var_dump(fastjson_pointer_set('{"x":1.5,"y":2}', '/y', 9));
?>
--EXPECT--
bool(false)
bool(true)
Inf and NaN cannot be JSON encoded
threw: Inf and NaN cannot be JSON encoded
string(15) "{"x":1.5,"y":9}"
