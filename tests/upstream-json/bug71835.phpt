--TEST--
Bug #71835 (fastjson_encode sometimes incorrectly detects recursion with JsonSerializable)
--EXTENSIONS--
fastjson
--FILE--
<?php
class SomeClass implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return [get_object_vars($this)];
    }
}
$class = new SomeClass;
$arr = [$class];
var_dump(fastjson_encode($arr));

class SomeClass2 implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return [(array)$this];
    }
}
$class = new SomeClass2;
$arr = [$class];
var_dump(fastjson_encode($arr));
?>
--EXPECT--
string(6) "[[[]]]"
string(6) "[[[]]]"
