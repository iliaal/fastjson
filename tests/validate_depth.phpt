--TEST--
fastjson_validate: $depth is enforced on the success path like ext/json
--EXTENSIONS--
fastjson
json
--SKIPIF--
<?php if (!function_exists('json_validate')) die('skip needs ext/json json_validate (PHP 8.3+)'); ?>

--FILE--
<?php

// Argument validation: $depth <= 0 / > INT_MAX raises ValueError.
try { fastjson_validate('"x"', 0); echo "no error\n"; }
catch (ValueError $e) { echo $e->getMessage(), "\n"; }

try { fastjson_validate('"x"', -1); echo "no error\n"; }
catch (ValueError $e) { echo $e->getMessage(), "\n"; }

try { fastjson_validate('"x"', PHP_INT_MAX); echo "no error\n"; }
catch (ValueError $e) { echo $e->getMessage(), "\n"; }

echo "---\n";

// Empty input + bad depth: ext/json short-circuits on the empty
// check before validating depth, returning false. fastjson must
// match -- raising ValueError here would be a divergence.
var_dump(fastjson_validate("", -1));
var_dump(fastjson_last_error() === FASTJSON_ERROR_SYNTAX);

echo "---\n";

// Depth required by input nesting (parity vs ext/json json_validate):
//   "1"        : depth >= 1
//   "[]"       : depth >= 2 (container costs 2 even when empty)
//   "[1]"      : depth >= 2
//   "[[1]]"    : depth >= 3
//   "[[[1]]]"  : depth >= 4
//   "{}"       : depth >= 2
//   '{"x":{}}' : depth >= 3
$inputs = ['1', '[]', '[1]', '[[1]]', '[[[1]]]', '{}', '{"x":{}}', '{"x":{"y":1}}'];
$mismatch = 0;
foreach ($inputs as $j) {
    for ($d = 1; $d <= 6; $d++) {
        $f = fastjson_validate($j, $d);
        $e = json_validate($j, $d);
        if ($f !== $e) {
            printf("MISMATCH %s d=%d: fast=%s ext=%s\n",
                $j, $d, var_export($f, true), var_export($e, true));
            $mismatch++;
        }
    }
}
echo "mismatches: $mismatch\n";

echo "---\n";

// Depth failure reports DEPTH, and braces inside strings do not count.
var_dump(fastjson_validate('[[[1]]]', 1));
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);
var_dump(fastjson_last_error_msg());
var_dump(fastjson_validate('[[[1]]]', 4));
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);
var_dump(fastjson_validate('{"s":"[[[["}', 2));
?>
--EXPECT--
fastjson_validate(): Argument #2 ($depth) must be greater than 0
fastjson_validate(): Argument #2 ($depth) must be greater than 0
fastjson_validate(): Argument #2 ($depth) must be less than 2147483647
---
bool(false)
bool(true)
---
mismatches: 0
---
bool(false)
bool(true)
string(28) "Maximum stack depth exceeded"
bool(true)
bool(true)
bool(true)
