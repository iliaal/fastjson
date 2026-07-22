--TEST--
fastjson_merge_patch enforces depth through the indexed object path
--EXTENSIONS--
fastjson
--FILE--
<?php

$data = [];
for ($i = 0; $i < 17; $i++) {
    $data["k$i"] = ['value' => $i];
}
$object = json_encode($data);

$result = fastjson_merge_patch('{}', $object, true, 2);
var_dump($result);
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);

$result = fastjson_merge_patch('{}', $object, true, 3);
var_dump(count($result) === 17 && $result['k16']['value'] === 16);
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

$result = fastjson_merge_patch($object, '{}', true, 2);
var_dump($result);
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);

$result = fastjson_merge_patch($object, '{}', true, 3);
var_dump(count($result) === 17 && $result['k16']['value'] === 16);
var_dump(fastjson_last_error() === JSON_ERROR_NONE);
?>
--EXPECT--
NULL
bool(true)
bool(true)
bool(true)
NULL
bool(true)
bool(true)
bool(true)
