--TEST--
fastjson_encode shares JsonSerializable recursion state with json_encode
--EXTENSIONS--
fastjson
--FILE--
<?php

final class NativeReentry implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return json_encode($this);
    }
}

$value = new NativeReentry();
$native = json_encode($value);
$fast = fastjson_encode($value);

var_dump($native);
var_dump($fast);
var_dump($fast === $native);
?>
--EXPECT--
string(5) "false"
string(5) "false"
bool(true)
