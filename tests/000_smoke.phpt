--TEST--
fastjson smoke: module loads and version matches phpversion()
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(extension_loaded('fastjson'));
var_dump(function_exists('fastjson_version'));
var_dump(fastjson_version() === phpversion('fastjson'));
echo fastjson_version(), "\n";
?>
--EXPECTF--
bool(true)
bool(true)
bool(true)
%s
