--TEST--
fastjson_encode() packed object props error-discard and destructor serialize_precision refresh
--EXTENSIONS--
fastjson
json
--FILE--
<?php
// Packed property HashTable (ArrayObject) hard error followed by UTF-8 error
foreach ([
    new ArrayObject([1, 2, INF, "\xFF"]),
    new ArrayObject([INF, "\xFF"]),
    new ArrayObject([1, INF, 2, 3, "\xFF"]),
] as $ao) {
    $je = json_encode($ao);
    $jerr = json_last_error();
    $fe = fastjson_encode($ao);
    $ferr = fastjson_last_error();
    var_dump($je === $fe, $jerr === $ferr, $ferr === FASTJSON_ERROR_UTF8);
}

// Temporary object returned by JsonSerializable mutates serialize_precision in __destruct
class PrecisionMutator {
    public function __destruct() {
        ini_set('serialize_precision', '3');
    }
}
class PrecisionBox implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new PrecisionMutator();
    }
}

$orig = ini_get('serialize_precision');
ini_set('serialize_precision', '-1');
$fe = fastjson_encode([new PrecisionBox(), 1.23456789]);
ini_set('serialize_precision', '-1');
$je = json_encode([new PrecisionBox(), 1.23456789]);
ini_set('serialize_precision', $orig);
var_dump($fe, $fe === $je);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
string(9) "[{},1.23]"
bool(true)
