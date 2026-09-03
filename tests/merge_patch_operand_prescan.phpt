--TEST--
fastjson_merge_patch: deeply-nested operands fail with DEPTH
--EXTENSIONS--
fastjson
--FILE--
<?php

// Deeply nested operands fail with DEPTH (via the depth-bounded merge),
// in both operand positions. $depth applies to the effective result, so
// a deep branch the patch discards still succeeds (see
// merge_patch_discarded_depth.phpt).
$deep = str_repeat('{"a":', 5000) . '1' . str_repeat('}', 5000);
var_dump(fastjson_merge_patch('{}', $deep, true, 512));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);
var_dump(fastjson_merge_patch($deep, '{}', true, 512));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

try {
    fastjson_merge_patch('{}', $deep, true, 512, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (JsonException $e) {
    echo "threw: ", $e->getCode(), "\n";
}

echo "---\n";

// Braces inside strings do not count toward nesting: a shallow
// document with thousands of string braces still merges. (The filler
// carries no double quotes: a literal " would end the JSON string.)
$braces = '{"s":"' . str_repeat('{{[', 2000) . '"}';
var_dump(is_array(fastjson_merge_patch('{}', $braces, true, 512)));
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);

// An escaped quote must not end the string skip: the real nesting
// here is 1, so depth 2 accepts.
$esc = '{"s":"a\\"b", "t":1}';
var_dump(fastjson_merge_patch('{}', $esc, true, 2) !== null);

echo "---\n";

// Boundary: N containers need depth >= N+1, matching fastjson_decode.
$nested3 = '{"a":{"b":{"c":1}}}';
var_dump(fastjson_merge_patch('{}', $nested3, true, 4) !== null);
var_dump(fastjson_merge_patch('{}', $nested3, true, 3));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

// Shallow invalid JSON still reports SYNTAX, not DEPTH.
var_dump(fastjson_merge_patch('{bad', '{}', true, 512));
var_dump(fastjson_last_error() === FASTJSON_ERROR_SYNTAX);
?>
--EXPECT--
NULL
bool(true)
NULL
bool(true)
threw: 1
---
bool(true)
bool(true)
bool(true)
---
bool(true)
NULL
bool(true)
NULL
bool(true)
