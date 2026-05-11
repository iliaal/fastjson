--TEST--
fastjson_encode: integer-valued doubles strip .0 by default; PRESERVE_ZERO_FRACTION keeps it
--EXTENSIONS--
fastjson
--FILE--
<?php

// Default: integer-valued doubles encode as integers (matches ext/json).
var_dump(fastjson_encode(0.0));
var_dump(fastjson_encode(1.0));
var_dump(fastjson_encode(-7.0));
var_dump(fastjson_encode(1230.0));
var_dump(fastjson_encode(1e15));     // boundary

echo "---\n";

// Non-integer doubles unaffected.
var_dump(fastjson_encode(3.14));
var_dump(fastjson_encode(-0.5));
var_dump(fastjson_encode(1.5));

echo "---\n";

// JSON_PRESERVE_ZERO_FRACTION: keep .0.
var_dump(fastjson_encode(0.0,    JSON_PRESERVE_ZERO_FRACTION));
var_dump(fastjson_encode(1.0,    JSON_PRESERVE_ZERO_FRACTION));
var_dump(fastjson_encode(1230.0, JSON_PRESERVE_ZERO_FRACTION));
var_dump(fastjson_encode(3.14,   JSON_PRESERVE_ZERO_FRACTION));   // no change
var_dump(fastjson_encode(-7.0,   JSON_PRESERVE_ZERO_FRACTION));

echo "---\n";

// Inside structures.
var_dump(fastjson_encode(["a" => 1.0, "b" => 2.5]));                              // string(15)
var_dump(fastjson_encode(["a" => 1.0, "b" => 2.5], JSON_PRESERVE_ZERO_FRACTION));  // string(17)

echo "---\n";

// Parity vs ext/json on the same inputs (including -0.0, which
// ext/json emits as "-0"/"-0.0"; casting via the long path would lose
// the sign).
$cases = [0.0, -0.0, 1.0, 1230.0, 3.14, -1.5];
foreach ($cases as $v) {
    if (fastjson_encode($v) !== json_encode($v)) {
        echo "default mismatch on ", var_export($v, true), "\n";
    }
    if (fastjson_encode($v, JSON_PRESERVE_ZERO_FRACTION)
            !== json_encode($v, JSON_PRESERVE_ZERO_FRACTION)) {
        echo "preserve mismatch on ", var_export($v, true), "\n";
    }
}
echo "parity OK\n";
?>
--EXPECT--
string(1) "0"
string(1) "1"
string(2) "-7"
string(4) "1230"
string(16) "1000000000000000"
---
string(4) "3.14"
string(4) "-0.5"
string(3) "1.5"
---
string(3) "0.0"
string(3) "1.0"
string(6) "1230.0"
string(4) "3.14"
string(4) "-7.0"
---
string(15) "{"a":1,"b":2.5}"
string(17) "{"a":1.0,"b":2.5}"
---
parity OK
