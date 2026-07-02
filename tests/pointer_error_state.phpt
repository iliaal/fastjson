--TEST--
fastjson_pointer_get/_exists: absent pointer clears error even under THROW
--EXTENSIONS--
fastjson
--FILE--
<?php

/* The stub documents an absent pointer as false/null + FASTJSON_ERROR_NONE.
 * JSON_THROW_ON_ERROR must not leak a prior error into that clean-miss
 * signal: after a failed decode leaves error 4, an absent lookup on
 * well-formed JSON has to report FASTJSON_ERROR_NONE (0), not the stale 4.
 * A genuine parse error still throws. */

function seed_error(): void {
    // A malformed decode records JSON_ERROR_SYNTAX in the global state.
    @fastjson_decode("{");
}

seed_error();
$e = fastjson_pointer_exists('{"a":1}', '/missing', JSON_THROW_ON_ERROR);
var_dump($e, fastjson_last_error());

seed_error();
$g = fastjson_pointer_get('{"a":1}', '/missing', false, 512, JSON_THROW_ON_ERROR);
var_dump($g, fastjson_last_error());

// Non-throw path already cleared at entry; still NONE.
seed_error();
var_dump(fastjson_pointer_exists('{"a":1}', '/missing'), fastjson_last_error());

// A found pointer still works and reports no error.
seed_error();
var_dump(fastjson_pointer_get('{"a":1}', '/a', false, 512, JSON_THROW_ON_ERROR));

// A genuinely malformed base document still throws under THROW.
try {
    fastjson_pointer_exists('{bad', '/x', JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (\JsonException $ex) {
    echo "threw: ", $ex->getCode(), "\n";
}
?>
--EXPECT--
bool(false)
int(0)
NULL
int(0)
bool(false)
int(0)
int(1)
threw: 4
