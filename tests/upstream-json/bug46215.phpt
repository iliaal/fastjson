--TEST--
Bug #46215 (fastjson_encode mutates its parameter and has some class-specific state)
--EXTENSIONS--
fastjson
--FILE--
<?php

class foo {
    protected $a = array();
}

$a = new foo;
$x = fastjson_encode($a);

print_r($a);

?>
--EXPECT--
foo Object
(
    [a:protected] => Array
        (
        )

)
