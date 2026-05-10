--TEST--
Bug #66025 (Indent wrong when fastjson_encode() called from jsonSerialize function)
--EXTENSIONS--
fastjson
--FILE--
<?php

class Foo implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return fastjson_encode([1], JSON_PRETTY_PRINT);
    }
}

echo fastjson_encode([new Foo]), "\n";
?>
--EXPECT--
["[\n    1\n]"]
