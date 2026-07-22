--TEST--
fastjson_merge_patch converts mutable UTF-8 and raw-number values under decode flags
--EXTENSIONS--
fastjson
--FILE--
<?php

$target = "{\"bad\xFF\":\"tar\xFFget\",\"targetBig\":999999999999999999999999,\"targetFloat\":1e309}";
$patch = "{\"new\xFF\":\"val\xFFue\",\"patchBig\":888888888888888888888888,\"patchFloat\":-1e309}";

$ignored = fastjson_merge_patch(
    $target,
    $patch,
    true,
    512,
    JSON_BIGINT_AS_STRING | JSON_INVALID_UTF8_IGNORE,
);
var_dump($ignored['bad'] === 'target');
var_dump($ignored['new'] === 'value');
var_dump($ignored['targetBig'] === '999999999999999999999999');
var_dump($ignored['patchBig'] === '888888888888888888888888');
var_dump($ignored['targetFloat'] === INF);
var_dump($ignored['patchFloat'] === -INF);

$replacement = "\xEF\xBF\xBD";
$substituted = fastjson_merge_patch(
    $target,
    $patch,
    true,
    512,
    JSON_BIGINT_AS_STRING | JSON_INVALID_UTF8_SUBSTITUTE,
);
var_dump($substituted["bad{$replacement}"] === "tar{$replacement}get");
var_dump($substituted["new{$replacement}"] === "val{$replacement}ue");
var_dump($substituted['targetBig'] === '999999999999999999999999');
var_dump($substituted['patchBig'] === '888888888888888888888888');
var_dump($substituted['targetFloat'] === INF);
var_dump($substituted['patchFloat'] === -INF);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
