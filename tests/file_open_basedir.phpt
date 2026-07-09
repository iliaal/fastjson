--TEST--
fastjson_file_decode/encode: open_basedir denial warns (like file_get_contents) and reports file failure
--EXTENSIONS--
fastjson
--FILE--
<?php

/* An open_basedir denial is surfaced two ways, matching
 * file_get_contents()/file_put_contents(): the plain-file stream wrapper
 * emits its own open_basedir warning (which fastjson does NOT suppress --
 * a security-boundary violation stays visible), AND the helper returns
 * null/false with fastjson_last_error() set. Other I/O failures (a missing
 * or unreadable file) stay silent; only open_basedir warns. The warnings
 * are asserted here rather than hidden with @, so the contract is tested. */

$base = sys_get_temp_dir() . '/fastjson_open_basedir_' . getmypid();
$allowed = $base . '/allowed';
$denied = $base . '/denied.json';

mkdir($base);
mkdir($allowed);
file_put_contents($denied, '{"x":1}');

ini_set('open_basedir', $allowed);

var_dump(fastjson_file_decode($denied, true));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
echo fastjson_last_error_msg(), "\n";

var_dump(fastjson_file_encode($denied, ['x' => 2]));
var_dump(fastjson_last_error() === JSON_ERROR_SYNTAX);
echo fastjson_last_error_msg(), "\n";

$allowedFile = $allowed . '/ok.json';
var_dump(fastjson_file_encode($allowedFile, ['ok' => true]));
var_dump(fastjson_file_decode($allowedFile, true));
?>
--EXPECTF--
Warning: fastjson_file_decode(): open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s) in %s on line %d
NULL
bool(true)
Failed to open file for reading

Warning: fastjson_file_encode(): open_basedir restriction in effect. File(%s) is not within the allowed path(s): (%s) in %s on line %d
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
