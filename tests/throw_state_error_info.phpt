--TEST--
JSON_THROW_ON_ERROR preserves the full fastjson error-info state
--EXTENSIONS--
fastjson
--FILE--
<?php

function poison_error_state(): array {
    fastjson_decode('{"a": bad}');
    return fastjson_last_error_info();
}

function assert_preserved(string $label, callable $cb): void {
    $before = poison_error_state();
    try {
        $cb();
        echo "$label: no throw\n";
    } catch (JsonException $e) {
        echo "$label: throw ", $e->getCode(), "\n";
    }
    $after = fastjson_last_error_info();
    echo "$label: ";
    var_dump($after === $before);
}

assert_preserved('encode', fn() => fastjson_encode(NAN, JSON_THROW_ON_ERROR));
assert_preserved('decode-depth', fn() => fastjson_decode('[[1]]', true, 2, JSON_THROW_ON_ERROR));

?>
--EXPECT--
encode: throw 7
encode: bool(true)
decode-depth: throw 1
decode-depth: bool(true)
