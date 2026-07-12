--TEST--
fastjson_decode caps recursive materialization when Zend has no stack-limit API
--EXTENSIONS--
fastjson
--SKIPIF--
<?php
if (PHP_VERSION_ID >= 80300) die('skip fallback applies to PHP before 8.3');
?>
--FILE--
<?php

$levels = 1100;
$json = str_repeat('[', $levels) . '0' . str_repeat(']', $levels);
$result = fastjson_decode($json, true, 2147483647);

var_dump($result);
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);
?>
--EXPECT--
NULL
bool(true)
