--TEST--
fastjson_validate: $depth argument validation matches ext/json (success-path walk not yet enforced)
--EXTENSIONS--
fastjson
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

// Note: $depth on the success path is intentionally NOT enforced.
// yyjson's validate-only mode has no parse-time depth flag and a
// post-parse walk would halve success-path throughput; validate
// returns true for valid JSON regardless of nesting given positive
// in-range $depth.
var_dump(fastjson_validate('[[[1]]]', 1));   // would be NULL on json_decode(d=1) but validate returns true
var_dump(fastjson_validate('[[[1]]]', 4));
var_dump(fastjson_validate('not json', 512));
?>
--EXPECTF--
fastjson_validate(): Argument #2 ($depth) must be greater than 0
fastjson_validate(): Argument #2 ($depth) must be greater than 0
fastjson_validate(): Argument #2 ($depth) must be less than %d
---
bool(true)
bool(true)
bool(false)
