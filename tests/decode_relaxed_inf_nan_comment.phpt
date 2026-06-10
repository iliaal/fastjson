--TEST--
fastjson_decode: RELAXED inf/nan prescan is comment-aware
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Regression: the inf/nan prescan that gates the exponent-overflow
 * ALLOW_INF_AND_NAN retry walked the input tracking only string state.
 * Under FASTJSON_DECODE_RELAXED (which enables JSONC comments) it
 * mistook comment bytes for tokens, two ways:
 *
 *  (1) false positive -- "info" in a comment read as an "Inf" literal,
 *      blocking the legitimate retry, so "1e309" decoded to null/SYNTAX
 *      instead of float(INF).
 *  (2) false negative -- an unbalanced quote inside a comment flipped
 *      the string state and hid a real bare Inf literal sitting outside
 *      the comment, so the retry wrongly accepted it as INF. */

$R = FASTJSON_DECODE_RELAXED;

// (1) Comment containing "info" must not block the 1e309 -> INF retry.
$r = fastjson_decode("/* info */ [1e309]", true, 512, $R);
var_dump($r);

// Line-comment form, same trap word.
$r = fastjson_decode("// more info\n[1e309]", true, 512, $R);
var_dump($r);

// (2) Unbalanced quote in a comment must not hide a bare Inf literal:
// ext/json rejects bare Inf, so this stays a SYNTAX error.
$r = fastjson_decode("// it's \"quoted\n[1e309, Inf]", true, 512, $R);
var_dump($r);
var_dump(fastjson_last_error() === FASTJSON_ERROR_SYNTAX);

// A genuine Inf/NaN literal (no comment) is still rejected under RELAXED.
$r = fastjson_decode("[Inf]", true, 512, $R);
var_dump($r);

// A real string value "Infinity" is untouched.
$r = fastjson_decode('/* c */ {"x": "Infinity"}', true, 512, $R);
var_dump($r);

// (3) A CR-terminated line comment must end at the CR, like yyjson, so a
// bare Inf after it is not swallowed as comment body and still fails.
$r = fastjson_decode("[1e309,//c\rInfinity]", true, 512, $R);
var_dump($r);
var_dump(fastjson_last_error() === FASTJSON_ERROR_SYNTAX);

// A legitimate CR-terminated comment ahead of an exponent-overflow
// number still decodes to INF.
$r = fastjson_decode("// hi\r[1e309]", true, 512, $R);
var_dump($r);
?>
--EXPECT--
array(1) {
  [0]=>
  float(INF)
}
array(1) {
  [0]=>
  float(INF)
}
NULL
bool(true)
NULL
array(1) {
  ["x"]=>
  string(8) "Infinity"
}
NULL
bool(true)
array(1) {
  [0]=>
  float(INF)
}
