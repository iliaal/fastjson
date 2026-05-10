--TEST--
fastjson_encode: JsonSerializable::jsonSerialize() replaces the object
--EXTENSIONS--
fastjson
--FILE--
<?php

class Box implements JsonSerializable {
    public function __construct(private string $name, private int $count) {}
    public function jsonSerialize(): array {
        return ["box" => $this->name, "n" => $this->count];
    }
}

$b = new Box("widgets", 5);
echo fastjson_encode($b), "\n";

// Returning a scalar is allowed.
class IntBox implements JsonSerializable {
    public function jsonSerialize(): mixed { return 42; }
}
echo fastjson_encode(new IntBox()), "\n";

// Returning $this triggers recursion detection.
class Loop implements JsonSerializable {
    public function jsonSerialize(): mixed { return $this; }
}
var_dump(fastjson_encode(new Loop()));
var_dump(fastjson_last_error() === FASTJSON_ERROR_RECURSION);

// Throwing in jsonSerialize bubbles up unchanged.
class Boom implements JsonSerializable {
    public function jsonSerialize(): mixed {
        throw new RuntimeException("custom failure");
    }
}
try {
    fastjson_encode(new Boom());
    echo "no exception\n";
} catch (RuntimeException $e) {
    echo "RuntimeException: ", $e->getMessage(), "\n";
}

// Nested: a JsonSerializable returning a structure containing more
// JsonSerializables resolves all the way down.
class Pair implements JsonSerializable {
    public function __construct(private mixed $a, private mixed $b) {}
    public function jsonSerialize(): array { return [$this->a, $this->b]; }
}
echo fastjson_encode(new Pair(new Box("inner", 1), 99)), "\n";

// Plain object (no JsonSerializable) still encodes via property hash.
class Plain {
    public string $name = "plain";
}
echo fastjson_encode(new Plain()), "\n";
?>
--EXPECT--
{"box":"widgets","n":5}
42
bool(false)
bool(true)
RuntimeException: custom failure
[{"box":"inner","n":1},99]
{"name":"plain"}
