--TEST--
fastjson_encode detects recursion through a hooked property
--EXTENSIONS--
fastjson
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80400) die('skip property hooks require PHP 8.4+');
?>
--FILE--
<?php

final class HookedCycle {
    public mixed $self {
        get => $this;
    }
}

$value = new HookedCycle();
$native = json_encode($value, 0, 8);
$nativeError = json_last_error();
$fast = fastjson_encode($value, 0, 8);
$fastError = fastjson_last_error();

var_dump($fast === $native);
var_dump($fastError === $nativeError);
var_dump($fastError === JSON_ERROR_RECURSION);

final class FirstHookedCycle {
    public mixed $next {
        get => $GLOBALS['secondHookedCycle'];
    }
}

final class SecondHookedCycle {
    public mixed $next {
        get => $GLOBALS['firstHookedCycle'];
    }
}

$firstHookedCycle = new FirstHookedCycle();
$secondHookedCycle = new SecondHookedCycle();
$native = json_encode($firstHookedCycle, 0, 8);
$nativeError = json_last_error();
$fast = fastjson_encode($firstHookedCycle, 0, 8);
$fastError = fastjson_last_error();

var_dump($fast === $native);
var_dump($fastError === $nativeError);
var_dump($fastError === JSON_ERROR_RECURSION);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
