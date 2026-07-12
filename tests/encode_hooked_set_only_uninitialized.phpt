--TEST--
fastjson_encode reads an uninitialized set-only property hook silently
--EXTENSIONS--
fastjson
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80400) die('skip property hooks require PHP 8.4+');
?>
--FILE--
<?php

final class UninitializedSetOnly {
    public string $value {
        set => $value;
    }
}

$value = new UninitializedSetOnly();
$native = json_encode($value);

try {
    $fast = fastjson_encode($value);
    var_dump($fast);
    var_dump($fast === $native);
} catch (Throwable $exception) {
    echo get_class($exception), ': ', $exception->getMessage(), "\n";
}
?>
--EXPECT--
string(14) "{"value":null}"
bool(true)
