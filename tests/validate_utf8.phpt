--TEST--
fastjson_validate: invalid UTF-8 inside a JSON string maps to JSON_ERROR_UTF8
--EXTENSIONS--
fastjson
--FILE--
<?php

// Valid UTF-8 inside a string passes.
var_dump(fastjson_validate('"héllo"'));
var_dump(fastjson_validate('"é"'));

echo "---\n";

// Lone continuation byte (0x80) inside a JSON string -- yyjson rejects
// as INVALID_STRING; we surface that as JSON_ERROR_UTF8.
$bad = "\"" . "\x80" . "\"";
var_dump(fastjson_validate($bad));
var_dump(fastjson_last_error() === FASTJSON_ERROR_UTF8);

echo "---\n";

// Parse-error byte classification at top level:
//  - Valid UTF-8 sequence that isn't valid JSON ("\xC3\xA9" = bare `é`)
//    stays SYNTAX, matching ext/json.
//  - Malformed UTF-8 (lone continuation, truncated lead) becomes UTF8.
$cases = [
    "\xC3\xA9"  => FASTJSON_ERROR_SYNTAX,
    "\x80"      => FASTJSON_ERROR_UTF8,
    "\xC3"      => FASTJSON_ERROR_UTF8,
    "[\x80]"    => FASTJSON_ERROR_UTF8,
    "[\xC3\xA9]" => FASTJSON_ERROR_SYNTAX,
];
foreach ($cases as $in => $expected) {
    fastjson_validate($in);
    var_dump(fastjson_last_error() === $expected);
}
?>
--EXPECT--
bool(true)
bool(true)
---
bool(false)
bool(true)
---
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
