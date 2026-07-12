--TEST--
fastjson_merge_patch matches raw malformed UTF-8 member names
--EXTENSIONS--
fastjson
--FILE--
<?php
$raw = "a\xFFb";

var_dump(fastjson_merge_patch(
    "{\"$raw\":1}",
    "{\"$raw\":null}",
    true,
    512,
    JSON_INVALID_UTF8_IGNORE,
));

var_dump(fastjson_merge_patch(
    '{"ab":1}',
    "{\"$raw\":null}",
    true,
    512,
    JSON_INVALID_UTF8_IGNORE,
));
?>
--EXPECT--
array(0) {
}
array(1) {
  ["ab"]=>
  int(1)
}
