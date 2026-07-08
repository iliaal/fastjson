--TEST--
fastjson_pointer_get: decode flags and depth are applied to the selected value
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_pointer_get('{"a":1,}', '/a', null, 512, FASTJSON_DECODE_RELAXED));

var_dump(fastjson_pointer_get(
    '{"n":999999999999999999999999}',
    '/n',
    null,
    512,
    JSON_BIGINT_AS_STRING
));

$arr = fastjson_pointer_get('{"o":{"x":1}}', '/o', null, 512, JSON_OBJECT_AS_ARRAY);
var_dump($arr);

$obj = fastjson_pointer_get('{"o":{"x":1}}', '/o', false, 512, JSON_OBJECT_AS_ARRAY);
var_dump(is_object($obj), $obj->x);

$badJson = '{"s":"a' . "\xFF" . 'b"}';
var_dump(
    fastjson_pointer_get($badJson, '/s', null, 512, JSON_INVALID_UTF8_IGNORE)
    === json_decode($badJson, true, 512, JSON_INVALID_UTF8_IGNORE)['s']
);
var_dump(
    fastjson_pointer_get($badJson, '/s', null, 512, JSON_INVALID_UTF8_SUBSTITUTE)
    === json_decode($badJson, true, 512, JSON_INVALID_UTF8_SUBSTITUTE)['s']
);

var_dump(fastjson_pointer_get('{"a":[1]}', '/a', true, 1));
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);

try {
    fastjson_pointer_get('{"a":[1]}', '/a', true, 1, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (JsonException $e) {
    echo "threw: ", $e->getCode(), " ", $e->getMessage(), "\n";
}
?>
--EXPECT--
int(1)
string(24) "999999999999999999999999"
array(1) {
  ["x"]=>
  int(1)
}
bool(true)
int(1)
bool(true)
bool(true)
NULL
bool(true)
threw: 1 Maximum stack depth exceeded
