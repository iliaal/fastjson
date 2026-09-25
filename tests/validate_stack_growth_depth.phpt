--TEST--
fastjson_validate: the validate reader's depth stack grows past its inline capacity
--EXTENSIONS--
fastjson
--FILE--
<?php
/* The validate-only reader (yyjson patch P-002) tracks open containers on a
 * heap stack that starts at 32 inline slots and doubles through realloc.
 * P-006 adds the overflow guards to that growth; these are the observable
 * consequences through the public path: documents that cross the inline
 * capacity and several doublings still validate, the $depth cap still wins
 * over the grown stack, and a syntax error in a deep document is still a
 * syntax error. The overflow arithmetic itself is covered by
 * validate_stack_growth_guard.phpt and validate_stack_growth_ilp32.phpt,
 * which drive the macro directly instead of allocating gigabytes. */

$boundary = 0;
foreach ([1, 31, 32, 33, 64, 65, 4096, 100000] as $nest) {
    $json = str_repeat('[', $nest) . str_repeat(']', $nest);
    $ok = fastjson_validate($json, $nest + 1);
    $err = fastjson_last_error();
    if ($ok !== true || $err !== FASTJSON_ERROR_NONE) $boundary++;
    printf("nest=%-6d valid=%-5s err=%d\n", $nest, var_export($ok, true), $err);
}
echo "boundary failures: $boundary\n";

echo "---\n";

/* Objects take the same push path. */
$mixed = str_repeat('{"a":', 2000) . '1' . str_repeat('}', 2000);
var_dump(fastjson_validate($mixed, 2001));
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);

echo "---\n";

/* The $depth cap is enforced after parsing, so a document far deeper than
 * the inline stack still reports DEPTH, not a parse failure. */
$deep = str_repeat('[', 100000) . str_repeat(']', 100000);
var_dump(fastjson_validate($deep));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);
var_dump(fastjson_last_error_msg());

echo "---\n";

/* 511 nested arrays plus a scalar is exactly the 512 cap. */
$edge = str_repeat('[', 511) . '1' . str_repeat(']', 511);
var_dump(fastjson_validate($edge));
var_dump(fastjson_validate($edge, 512));
var_dump(fastjson_validate($edge, 511));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);
var_dump(fastjson_validate($edge));
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);

echo "---\n";

/* A malformed deep document keeps reporting the parse error. */
var_dump(fastjson_validate(str_repeat('[', 5000) . 'x' . str_repeat(']', 5000)));
var_dump(fastjson_last_error() === FASTJSON_ERROR_SYNTAX);
?>
--EXPECT--
nest=1      valid=true  err=0
nest=31     valid=true  err=0
nest=32     valid=true  err=0
nest=33     valid=true  err=0
nest=64     valid=true  err=0
nest=65     valid=true  err=0
nest=4096   valid=true  err=0
nest=100000 valid=true  err=0
boundary failures: 0
---
bool(true)
bool(true)
---
bool(false)
bool(true)
string(28) "Maximum stack depth exceeded"
---
bool(true)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
---
bool(false)
bool(true)
