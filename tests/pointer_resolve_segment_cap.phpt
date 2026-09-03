--TEST--
fastjson_pointer_get/_exists: resolve enforces the depth gate plus an absolute segment cap
--EXTENSIONS--
fastjson
--FILE--
<?php

// At/above $depth segments the resolve fails like plan_init, with DEPTH.
$ptr512 = str_repeat('/a', 512);
var_dump(fastjson_pointer_get('{"a":1}', $ptr512, true, 512));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

// Just under the gate the pointer is walked normally: a path that
// leaves the document is absent, not an error.
$ptr511 = str_repeat('/a', 511);
var_dump(fastjson_pointer_get('{"a":1}', $ptr511, true, 512));
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);

echo "---\n";

// Above the absolute segment cap the $depth value no longer matters.
$huge = str_repeat('/a', 5000);
var_dump(fastjson_pointer_get('{"a":1}', $huge, true, 100000));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);
var_dump(fastjson_pointer_exists('{"a":1}', $huge));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

try {
    fastjson_pointer_get('{"a":1}', $huge, true, 100000, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (JsonException $e) {
    echo "threw: ", $e->getCode(), "\n";
}

echo "---\n";

// Ordinary pointers are unaffected.
var_dump(fastjson_pointer_exists('{"a":1}', '/a'));
var_dump(fastjson_pointer_exists('{"a":1}', '/missing'));
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);
var_dump(fastjson_pointer_get('{"a":1}', '/a', true));
?>
--EXPECT--
NULL
bool(true)
NULL
bool(true)
---
NULL
bool(true)
bool(false)
bool(true)
threw: 1
---
bool(true)
bool(false)
bool(true)
int(1)
