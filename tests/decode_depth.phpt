--TEST--
fastjson_decode: $depth bounds match ext/json verbatim
--EXTENSIONS--
fastjson
json
--FILE--
<?php

// Argument validation: $depth <= 0 raises ValueError; $depth > INT_MAX too.
try { fastjson_decode('"x"', true, 0); echo "no error\n"; }
catch (ValueError $e) { echo $e->getMessage(), "\n"; }

try { fastjson_decode('"x"', true, -1); echo "no error\n"; }
catch (ValueError $e) { echo $e->getMessage(), "\n"; }

try { fastjson_decode('"x"', true, PHP_INT_MAX); echo "no error\n"; }
catch (ValueError $e) { echo $e->getMessage(), "\n"; }

echo "---\n";

// Depth required by input nesting (parity vs ext/json):
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
        $f = fastjson_decode($j, true, $d);
        $e = json_decode($j, true, $d);
        if ($f !== $e) {
            printf("MISMATCH %s d=%d: fast=%s ext=%s\n",
                $j, $d, var_export($f, true), var_export($e, true));
            $mismatch++;
        }
    }
}
echo "mismatches: $mismatch\n";

echo "---\n";

// On overflow: fastjson_last_error reports DEPTH, message reads
// "Maximum stack depth exceeded".
fastjson_decode('[[1]]', true, 2);
var_dump(fastjson_last_error() === FASTJSON_ERROR_DEPTH);
var_dump(fastjson_last_error_msg());

// Successful decode resets the error.
fastjson_decode('[[1]]', true, 3);
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);
?>
--EXPECTF--
fastjson_decode(): Argument #3 ($depth) must be greater than 0
fastjson_decode(): Argument #3 ($depth) must be greater than 0
fastjson_decode(): Argument #3 ($depth) must be less than %d
---
mismatches: 0
---
bool(true)
string(28) "Maximum stack depth exceeded"
bool(true)
