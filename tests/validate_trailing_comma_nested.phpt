--TEST--
fastjson_validate rejects a trailing comma after nested containers at decode's position
--EXTENSIONS--
fastjson
--FILE--
<?php

$cases = [
    '[[],]',
    '[{},]',
    '[{"a":1},]',
    '[[1],[2],]',
    '[[1,],',
    '{"a":[1,],}',
    '[[1,],1]',
];

foreach ($cases as $input) {
    $ok = fastjson_validate($input);
    $v = [fastjson_last_error(), fastjson_last_error_msg(), fastjson_last_error_pos()];
    fastjson_decode($input);
    $d = [fastjson_last_error(), fastjson_last_error_msg(), fastjson_last_error_pos()];
    if ($v !== $d) {
        echo "MISMATCH $input: ", json_encode($v), " vs ", json_encode($d), "\n";
    }
    if (function_exists('json_validate') && json_validate($input) !== $ok) {
        echo "JSON_VALIDATE MISMATCH $input\n";
    }
    printf("%s %s pos=%d %s\n", $input, $ok ? 'ok' : 'invalid', $v[2], $v[1]);
}

var_dump(fastjson_validate('[[],[]]'), fastjson_validate('[{"a":[]},{}]'));
?>
--EXPECT--
[[],] invalid pos=3 trailing comma is not allowed
[{},] invalid pos=3 trailing comma is not allowed
[{"a":1},] invalid pos=8 trailing comma is not allowed
[[1],[2],] invalid pos=8 trailing comma is not allowed
[[1,], invalid pos=3 trailing comma is not allowed
{"a":[1,],} invalid pos=7 trailing comma is not allowed
[[1,],1] invalid pos=3 trailing comma is not allowed
bool(true)
bool(true)
