--TEST--
fastjson_merge_patch: deep operand hits the depth cap instead of crashing
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Regression: yyjson_merge_patch and yyjson_mut_doc_imut_copy recurse
 * once per nesting level on the C stack, BEFORE the depth-capped zval
 * walker runs. A deeply nested patch therefore overflowed the stack
 * (SIGSEGV) before the $depth guard could fire. The fix bounds the structural
 * nesting of every operand that the merge/copy path still has to traverse. */

$n = 200000;
$patch = str_repeat('{"a":', $n) . '1' . str_repeat('}', $n);
$cap = str_repeat('{"a":', 1100) . '1' . str_repeat('}', 1100);

// Deep patch, shallow target: must fail with DEPTH, not crash.
$r = fastjson_merge_patch('{}', $patch, true, 512);
var_dump($r);
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

// Deep target, shallow patch: same guard covers the target operand.
$r = fastjson_merge_patch($patch, '{}', true, 512);
var_dump($r);
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

// A non-object patch replaces the target wholesale, so the target doesn't need
// the stack-depth prewalk that protects recursive object merges.
$r = fastjson_merge_patch($cap, '0', true, 100000);
var_dump($r);
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);

// THROW_ON_ERROR surfaces the depth failure as an exception.
try {
    fastjson_merge_patch('{}', $patch, true, 512, JSON_THROW_ON_ERROR);
    echo "no exception\n";
} catch (\JsonException $e) {
    echo "threw: ", $e->getMessage(), "\n";
}

// User $depth above the stack-safe cap must not bypass the pre-check.
$r = fastjson_merge_patch('{}', $cap, true, 100000);
var_dump($r);
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

// A stray quote/brace inside a RELAXED comment must not let a deep
// operand slip past the guard. The depth is measured on the parsed
// tree, so source-level comments and quotes can't undercount it.
$bypass = '/* " { [ */ ' . $patch;
$r = fastjson_merge_patch('{}', $bypass, true, 512, FASTJSON_DECODE_RELAXED);
var_dump($r);
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);
$r = fastjson_merge_patch($bypass, '{}', true, 512, FASTJSON_DECODE_RELAXED);
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);

// A nesting within the limit still merges normally.
$ok = str_repeat('{"a":', 50) . '1' . str_repeat('}', 50);
var_dump(fastjson_merge_patch('{}', $ok, true, 512) !== null);

// RELAXED with a comment and an in-limit depth still merges.
var_dump(fastjson_merge_patch('{}', '/* c */ ' . $ok, true, 512, FASTJSON_DECODE_RELAXED) !== null);

// Plain RFC 7386 behaviour is unchanged.
echo fastjson_encode(
    fastjson_merge_patch('{"a":1,"b":2}', '{"b":null,"c":3}', true)
), "\n";
?>
--EXPECT--
NULL
bool(true)
NULL
bool(true)
int(0)
bool(true)
threw: Maximum stack depth exceeded
NULL
bool(true)
NULL
bool(true)
bool(true)
bool(true)
bool(true)
{"a":1,"c":3}
