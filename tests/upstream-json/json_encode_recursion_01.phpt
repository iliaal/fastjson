--TEST--
fastjson_encode() Recursion test with just JsonSerializable
--EXTENSIONS--
fastjson
--FILE--
<?php

class SerializingTest implements JsonSerializable
{
    public $a = 1;

    private $b = 'hide';

    protected $c = 'protect';

    public function jsonSerialize(): mixed
    {
        $result = fastjson_encode($this);
        var_dump($result);
        return $this;
    }
}

var_dump(fastjson_encode(new SerializingTest()));
?>
--EXPECT--
bool(false)
string(7) "{"a":1}"
