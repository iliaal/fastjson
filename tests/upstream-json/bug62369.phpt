--TEST--
FR #62369 (Segfault on fastjson_encode(deeply_nested_array)
--EXTENSIONS--
fastjson
--FILE--
<?php

$array = array();
for ($i=0; $i < 550; $i++) {
    $array = array($array);
}

fastjson_encode($array, 0, 551);
switch (fastjson_last_error()) {
    case JSON_ERROR_NONE:
        echo 'OK' . PHP_EOL;
    break;
    case JSON_ERROR_DEPTH:
        echo 'ERROR' . PHP_EOL;
    break;
}

fastjson_encode($array, 0, 540);
switch (fastjson_last_error()) {
    case JSON_ERROR_NONE:
        echo 'OK' . PHP_EOL;
    break;
    case JSON_ERROR_DEPTH:
        echo 'ERROR' . PHP_EOL;
    break;
}
?>
--EXPECT--
OK
ERROR
