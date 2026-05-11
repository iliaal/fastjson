--TEST--
fastjson_encode: invalid UTF-8 in object key fails cleanly under default / PARTIAL / THROW
--EXTENSIONS--
fastjson
--FILE--
<?php

$bad = ["\xFF" => 1];

// Default: false + JSON_ERROR_UTF8 (5), no malformed output.
$r = fastjson_encode($bad);
var_dump($r);
var_dump(fastjson_last_error());

// PARTIAL_OUTPUT_ON_ERROR: ext/json substitutes an empty quoted key.
var_dump(fastjson_encode($bad, JSON_PARTIAL_OUTPUT_ON_ERROR));
var_dump(fastjson_last_error());

// THROW_ON_ERROR: JsonException, code 5.
try {
    fastjson_encode($bad, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (JsonException $e) {
    var_dump($e->getCode());
    var_dump($e->getMessage());
}

// Match ext/json byte-for-byte on the PARTIAL substitution.
var_dump(
    fastjson_encode($bad, JSON_PARTIAL_OUTPUT_ON_ERROR)
        === json_encode($bad, JSON_PARTIAL_OUTPUT_ON_ERROR)
);

// Sanity: a valid ASCII key still works after a prior failure.
var_dump(fastjson_encode(["ok" => 1]));
?>
--EXPECT--
bool(false)
int(5)
string(6) "{"":1}"
int(5)
int(5)
string(56) "Malformed UTF-8 characters, possibly incorrectly encoded"
bool(true)
string(8) "{"ok":1}"
