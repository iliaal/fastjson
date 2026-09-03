--TEST--
fastjson: THROW_ON_ERROR throws JsonException; fallback owned when ext/json absent
--EXTENSIONS--
fastjson
json
--FILE--
<?php

// With ext/json present its JsonException serves the throw path; the
// fastjson-owned fallback is registered only when ext/json is absent.
var_dump(class_exists('JsonException'));
var_dump(is_subclass_of('JsonException', 'Exception'));
var_dump(class_exists('Fastjson\\JsonException'));

try {
    fastjson_decode('{bad}', true, 512, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (JsonException $e) {
    var_dump($e->getCode() === FASTJSON_ERROR_SYNTAX);
}
?>
--EXPECT--
bool(true)
bool(true)
bool(false)
bool(true)
