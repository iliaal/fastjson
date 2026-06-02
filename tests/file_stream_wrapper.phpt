--TEST--
fastjson_file_decode/encode: route through the PHP streams layer (wrappers honored)
--EXTENSIONS--
fastjson
--FILE--
<?php

// A raw stdio implementation could not open a file:// URL; the streams
// layer can. Round-trip through an explicit wrapper to prove routing.
$path = sys_get_temp_dir() . '/fastjson_file_wrapper.json';
$url  = 'file://' . $path;

$data = ['k' => 'v', 'n' => 42];

var_dump(fastjson_file_encode($url, $data));
var_dump(fastjson_file_decode($url, true) === $data);

// And the bytes are on disk at the plain path too.
var_dump(fastjson_file_decode($path, true) === $data);

unlink($path);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
