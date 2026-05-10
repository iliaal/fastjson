--TEST--
Bug #73113 (Segfault with throwing JsonSerializable)
Also test that the custom exception is not wrapped by ext/json
--EXTENSIONS--
fastjson
--FILE--
<?php

class JsonSerializableObject implements \JsonSerializable
{
    public function jsonSerialize(): mixed
    {
        throw new \Exception('This error is expected');
    }
}

$obj = new JsonSerializableObject();
try {
    echo fastjson_encode($obj);
} catch (\Exception $e) {
    echo $e->getMessage();
}
?>
--EXPECT--
This error is expected
