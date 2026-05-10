--TEST--
fastjson_encode: JSON_THROW_ON_ERROR raises on encode failure
--EXTENSIONS--
fastjson
--FILE--
<?php

// Without the flag: returns false, populates last_error, no exception.
$r = fastjson_encode(INF);
var_dump($r);
var_dump(fastjson_last_error() === FASTJSON_ERROR_INF_OR_NAN);

// With the flag: throws.
try {
    fastjson_encode(INF, JSON_THROW_ON_ERROR);
    echo "no exception\n";
} catch (JsonException $e) {
    echo "caught: ", $e->getMessage(), " (code ", $e->getCode(), ")\n";
}

// Recursion under THROW_ON_ERROR.
try {
    $a = [1]; $a[] = &$a;
    fastjson_encode($a, JSON_THROW_ON_ERROR);
    echo "no exception\n";
} catch (JsonException $e) {
    echo "caught: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
bool(false)
bool(true)
caught: Inf and NaN cannot be JSON encoded (code 7)
caught: Recursion detected
