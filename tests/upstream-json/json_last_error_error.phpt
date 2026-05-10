--TEST--
fastjson_last_error() failures
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_last_error());

try {
    var_dump(fastjson_last_error(true));
} catch (TypeError $e) {
    echo $e->getMessage(), "\n";
}

?>
--EXPECT--
int(0)
fastjson_last_error() expects exactly 0 arguments, 1 given
