--TEST--
Bug #72069 (Behavior \JsonSerializable different from fastjson_encode)
--EXTENSIONS--
fastjson
--FILE--
<?php

$result = fastjson_encode(['end' => fastjson_decode('', true)]);
var_dump($result);

class A implements \JsonSerializable
{
    function jsonSerialize(): mixed
    {
        return ['end' => fastjson_decode('', true)];
    }
}
$a = new A();
$toJsonData = $a->jsonSerialize();
$result = fastjson_encode($a);
var_dump($result);

$result = fastjson_encode($toJsonData);
var_dump($result);
?>
--EXPECT--
string(12) "{"end":null}"
string(12) "{"end":null}"
string(12) "{"end":null}"
