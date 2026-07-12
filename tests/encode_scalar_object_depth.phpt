--TEST--
fastjson_encode does not charge container depth for scalar JsonSerializable results or backed enums
--EXTENSIONS--
fastjson
--FILE--
<?php

final class ScalarSerializable implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return 42;
    }
}

enum ScalarBacked: string {
    case Value = 'value';
}

$cases = [
    [new ScalarSerializable()],
    [ScalarBacked::Value],
];

foreach ($cases as $value) {
    var_dump(fastjson_encode($value, 0, 1) === json_encode($value, 0, 1));
}

var_dump(
    fastjson_encode(new ScalarSerializable(), 0, 0)
    === json_encode(new ScalarSerializable(), 0, 0)
);
var_dump(
    fastjson_encode(ScalarBacked::Value, 0, 0)
    === json_encode(ScalarBacked::Value, 0, 0)
);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
