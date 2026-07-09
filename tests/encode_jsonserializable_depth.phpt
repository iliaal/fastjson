--TEST--
fastjson_encode: JsonSerializable adds no depth level of its own (matches ext/json)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* ext/json's serializable path does not increment encoder->depth; only the
 * returned value's containers do. So a jsonSerialize() returning an M-deep
 * structure needs $depth >= M, exactly as returning that structure directly.
 * fastjson previously charged the serialize call an extra level, rejecting
 * valid input one level too shallow -- for both the returns-$this case and
 * the returns-a-fresh-value case. */

class RetThis implements JsonSerializable {
    public int $a = 1;
    public array $b = [2];              // one level of nesting in the props
    public function jsonSerialize(): mixed { return $this; }
}
class RetNestedArr implements JsonSerializable {
    public function jsonSerialize(): mixed { return [[2]]; }   // nesting 2
}
class RetNestedObj implements JsonSerializable {
    public function jsonSerialize(): mixed { return (object)["b" => [2]]; }
}

foreach ([new RetThis(), new RetNestedArr(), new RetNestedObj()] as $o) {
    // Same tight $depth that ext/json accepts must be accepted here.
    $d = 2;
    $e = json_encode($o, 0, $d);
    $f = fastjson_encode($o, 0, $d);
    var_dump($e === $f);
    echo $f, "\n";
}

// One level too shallow still fails, matching ext/json.
var_dump(fastjson_encode(new RetNestedArr(), 0, 1));
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);
var_dump(json_encode(new RetNestedArr(), 0, 1));
?>
--EXPECT--
bool(true)
{"a":1,"b":[2]}
bool(true)
[[2]]
bool(true)
{"b":[2]}
bool(false)
bool(true)
bool(false)
