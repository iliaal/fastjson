--TEST--
fastjson_decode / fastjson_validate: overflow + unquoted Inf/NaN literal rejects (no over-acceptance)
--EXTENSIONS--
fastjson
--FILE--
<?php

// The overflow-retry path must NOT also accept unquoted Inf/NaN
// literals appearing elsewhere in the same input. ext/json rejects
// these mixed inputs entirely.

// json_validate() is 8.3+; fall back to json_decode parity on 8.1/8.2.
$ext_json_validate = function (string $json): bool {
    if (function_exists('json_validate')) {
        return json_validate($json);
    }
    json_decode($json);
    return json_last_error() === JSON_ERROR_NONE;
};

foreach ([
    '[1e309, NaN]',
    '[1e309, Infinity]',
    '[1e309, -Infinity]',
    '[1e309, inf]',
    '[1e309, nan]',
    '[NaN, 1e309]',
    '{"a": 1e309, "b": NaN}',
] as $j) {
    $validate = fastjson_validate($j);
    $decode = fastjson_decode($j);
    printf("%-26s validate=%s decode_is_null=%s ext/json_validate=%s\n",
        $j,
        var_export($validate, true),
        var_export($decode === null, true),
        var_export($ext_json_validate($j), true));
}

// Sanity: plain overflow still works.
var_dump(fastjson_decode('[1e309, 2]'));

// Sanity: quoted "Infinity" / "NaN" strings stay valid.
var_dump(fastjson_decode('{"k":1e309,"name":"Infinity"}', true));
?>
--EXPECT--
[1e309, NaN]               validate=false decode_is_null=true ext/json_validate=false
[1e309, Infinity]          validate=false decode_is_null=true ext/json_validate=false
[1e309, -Infinity]         validate=false decode_is_null=true ext/json_validate=false
[1e309, inf]               validate=false decode_is_null=true ext/json_validate=false
[1e309, nan]               validate=false decode_is_null=true ext/json_validate=false
[NaN, 1e309]               validate=false decode_is_null=true ext/json_validate=false
{"a": 1e309, "b": NaN}     validate=false decode_is_null=true ext/json_validate=false
array(2) {
  [0]=>
  float(INF)
  [1]=>
  int(2)
}
array(2) {
  ["k"]=>
  float(INF)
  ["name"]=>
  string(8) "Infinity"
}
