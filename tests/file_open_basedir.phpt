--TEST--
fastjson_file_decode/encode: open_basedir denial reports file failure
--EXTENSIONS--
fastjson
--FILE--
<?php

$base = sys_get_temp_dir() . '/fastjson_open_basedir_' . getmypid();
$allowed = $base . '/allowed';
$denied = $base . '/denied.json';

mkdir($base);
mkdir($allowed);
file_put_contents($denied, '{"x":1}');

ini_set('open_basedir', $allowed);

var_dump(@fastjson_file_decode($denied, true));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
echo fastjson_last_error_msg(), "\n";

var_dump(@fastjson_file_encode($denied, ['x' => 2]));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
echo fastjson_last_error_msg(), "\n";

$allowedFile = $allowed . '/ok.json';
var_dump(fastjson_file_encode($allowedFile, ['ok' => true]));
var_dump(fastjson_file_decode($allowedFile, true));
?>
--EXPECT--
NULL
bool(true)
Failed to open file for reading
bool(false)
bool(true)
Failed to open file for writing
bool(true)
array(1) {
  ["ok"]=>
  bool(true)
}
--CLEAN--
<?php
foreach (glob(sys_get_temp_dir() . '/fastjson_open_basedir_*') as $dir) {
    @unlink($dir . '/allowed/ok.json');
    @rmdir($dir . '/allowed');
    @unlink($dir . '/denied.json');
    @rmdir($dir);
}
?>
