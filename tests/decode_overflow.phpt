--TEST--
fastjson_decode / fastjson_validate: exponent-overflow numbers decode to INF (matches ext/json)
--EXTENSIONS--
fastjson
--FILE--
<?php

// Plain overflow: ext/json accepts and returns INF; we now match.
var_dump(fastjson_decode("1e309"));
var_dump(fastjson_last_error());
var_dump(fastjson_decode("-1e309"));
var_dump(fastjson_decode("[1e309, 2]"));
var_dump(fastjson_decode("{\"big\": 1e309}", true));

// Validate also accepts.
var_dump(fastjson_validate("1e309"));
var_dump(fastjson_validate("[1e309, 2]"));

// Literal Infinity / -Infinity / NaN are NOT valid JSON and ext/json
// rejects them; fastjson must too.
var_dump(fastjson_decode("Infinity"));
var_dump(fastjson_last_error() !== 0);
var_dump(fastjson_decode("-Infinity"));
var_dump(fastjson_last_error() !== 0);
var_dump(fastjson_decode("NaN"));
var_dump(fastjson_last_error() !== 0);

// Validate rejects literals too.
var_dump(fastjson_validate("Infinity"));
var_dump(fastjson_validate("NaN"));
?>
--EXPECT--
float(INF)
int(0)
float(-INF)
array(2) {
  [0]=>
  float(INF)
  [1]=>
  int(2)
}
array(1) {
  ["big"]=>
  float(INF)
}
bool(true)
bool(true)
NULL
bool(true)
NULL
bool(true)
NULL
bool(true)
bool(false)
bool(false)
