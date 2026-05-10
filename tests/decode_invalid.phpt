--TEST--
fastjson_decode: parse failure returns null and populates last_error
--EXTENSIONS--
fastjson
--FILE--
<?php

// Decoded null is indistinguishable from parse-failure null on its own;
// callers must check fastjson_last_error() to distinguish.
$ok = fastjson_decode('null');
var_dump($ok);
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);

echo "---\n";

// Parse failure -> null + populated last_error.
$bad = fastjson_decode('not json');
var_dump($bad);
var_dump(fastjson_last_error() === FASTJSON_ERROR_SYNTAX);
var_dump(strlen(fastjson_last_error_msg()) > 0);
var_dump(fastjson_last_error_msg() !== 'No error');

echo "---\n";

// Successful decode after a failure resets the error.
$arr = fastjson_decode('[1,2,3]');
var_dump($arr);
var_dump(fastjson_last_error());
var_dump(fastjson_last_error_msg());

echo "---\n";

// Various malformed inputs all map to SYNTAX.
foreach (['', '{', '[1,]', '{"a":}', 'undefined', '"unterminated'] as $bad) {
    $r = fastjson_decode($bad);
    if ($r !== null || fastjson_last_error() !== FASTJSON_ERROR_SYNTAX) {
        echo "UNEXPECTED for input ", var_export($bad, true), "\n";
    }
}
echo "all malformed inputs reported as SYNTAX\n";
?>
--EXPECT--
NULL
bool(true)
---
NULL
bool(true)
bool(true)
bool(true)
---
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
int(0)
string(8) "No error"
---
all malformed inputs reported as SYNTAX
