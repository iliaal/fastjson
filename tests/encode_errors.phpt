--TEST--
fastjson_encode: INF/NAN, recursion, depth overflow, unsupported types
--EXTENSIONS--
fastjson
--FILE--
<?php

// INF and NaN.
var_dump(fastjson_encode(INF));
var_dump(fastjson_last_error() === FASTJSON_ERROR_INF_OR_NAN);

var_dump(fastjson_encode(-INF));
var_dump(fastjson_last_error() === FASTJSON_ERROR_INF_OR_NAN);

var_dump(fastjson_encode(NAN));
var_dump(fastjson_last_error() === FASTJSON_ERROR_INF_OR_NAN);

echo "---\n";

// Self-referential array.
$a = [1, 2];
$a[] = &$a;
var_dump(fastjson_encode($a));
var_dump(fastjson_last_error() === FASTJSON_ERROR_RECURSION);

// Two-step cycle: o.next -> p, p.next -> o.
$o = new stdClass(); $p = new stdClass();
$o->next = $p; $p->next = $o;
var_dump(fastjson_encode($o));
var_dump(fastjson_last_error() === FASTJSON_ERROR_RECURSION);

echo "---\n";

// Depth cap.
$nested = [];
$ref = &$nested;
for ($i = 0; $i < 600; $i++) {
    $ref[] = [];
    $ref = &$ref[0];
}
$ref = "leaf";
unset($ref);

var_dump(fastjson_encode($nested, 0, 5));    // shallow cap -> overflow
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

var_dump(is_string(fastjson_encode($nested, 0, 1024)));   // tall cap -> ok
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);

echo "---\n";

// Unsupported type (resource).
$r = fopen('php://memory', 'r');
var_dump(fastjson_encode($r));
var_dump(fastjson_last_error() === FASTJSON_ERROR_UNSUPPORTED_TYPE);
fclose($r);

echo "---\n";

// $depth=0 is NOT a ValueError on encode (matches ext/json bug81532):
// encode lazily fails on the first container instead.
var_dump(fastjson_encode([1, 2], 0, 0));            // false: container at depth 0 fails
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);
var_dump(fastjson_encode(42, 0, 0));                // 42: scalar at depth 0 still encodes (no container -> no level needed)
?>
--EXPECT--
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
---
bool(false)
bool(true)
bool(false)
bool(true)
---
bool(false)
bool(true)
bool(true)
bool(true)
---
bool(false)
bool(true)
---
bool(false)
bool(true)
string(2) "42"
