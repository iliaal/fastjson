--TEST--
fastjson_decode: JSON_THROW_ON_ERROR preserves prior global error state on success
--EXTENSIONS--
fastjson
--FILE--
<?php

// Poison the error state from a prior failed call.
fastjson_decode("bogus");
$poison = fastjson_last_error();
var_dump($poison !== 0);

// A successful decode under THROW_ON_ERROR must NOT clear the state.
$r = fastjson_decode("[1, 2, 3]", true, 512, JSON_THROW_ON_ERROR);
var_dump($r);
var_dump(fastjson_last_error() === $poison);

// A successful decode WITHOUT throw mode clears as usual.
$r = fastjson_decode("[4]", true);
var_dump($r);
var_dump(fastjson_last_error());

// A failing decode under THROW_ON_ERROR throws AND keeps prior state.
fastjson_decode("bogus");                // re-poison
$snapshot = fastjson_last_error();
try {
    fastjson_decode("garbage", true, 512, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (JsonException $e) {
    var_dump($e->getCode() !== 0);
}
var_dump(fastjson_last_error() === $snapshot);
?>
--EXPECT--
bool(true)
array(3) {
  [0]=>
  int(1)
  [1]=>
  int(2)
  [2]=>
  int(3)
}
bool(true)
array(1) {
  [0]=>
  int(4)
}
int(0)
bool(true)
bool(true)
