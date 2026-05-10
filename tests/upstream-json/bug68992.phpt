--TEST--
Bug #68992 (fastjson_encode stacks exceptions thrown by JsonSerializable classes)
--EXTENSIONS--
fastjson
--FILE--
<?php

class MyClass implements JsonSerializable {
    public function jsonSerialize(): mixed {
        throw new Exception('Not implemented!');
    }
}
$classes = [];
for($i = 0; $i < 5; $i++) {
    $classes[] = new MyClass();
}

try {
    fastjson_encode($classes);
} catch(Exception $e) {
    do {
        printf("%s (%d) [%s]\n", $e->getMessage(), $e->getCode(), get_class($e));
    } while ($e = $e->getPrevious());
}
?>
--EXPECT--
Not implemented! (0) [Exception]
