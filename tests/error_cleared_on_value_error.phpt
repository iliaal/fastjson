--TEST--
fastjson_decode/validate: last_error cleared before argument ValueError raises
--EXTENSIONS--
fastjson
--FILE--
<?php

// ext/json clears the per-request error state before checking $depth,
// so a ValueError thrown for bad $depth leaves last_error == NONE.
// fastjson must mirror that contract.

function assert_cleared(string $where, Closure $bad_call): void {
    fastjson_validate('not json');  // populate failure state
    assert(fastjson_last_error() === FASTJSON_ERROR_SYNTAX, "setup");
    try { $bad_call(); echo "$where: no throw\n"; }
    catch (ValueError $_) {
        echo "$where last_error: ", fastjson_last_error(), "\n";
    }
}

assert_cleared("validate depth=0",  fn() => fastjson_validate("{}", 0));
assert_cleared("validate depth=-1", fn() => fastjson_validate("{}", -1));
assert_cleared("decode depth=0",    fn() => fastjson_decode("{}", null, 0));
assert_cleared("decode depth=-1",   fn() => fastjson_decode("{}", null, -1));

// THROW_ON_ERROR mode is documented to preserve prior global state;
// the argument ValueError isn't a JsonException, but the existing
// "snapshot at entry, restore on JsonException" contract means
// non-throw clearing must not bleed into throw-mode callers either.
// Sanity check: a successful decode under THROW_ON_ERROR sees prior
// state preserved.
fastjson_validate('not json');
$pre = fastjson_last_error();
fastjson_decode('"ok"', null, 512, JSON_THROW_ON_ERROR);
echo "throw-mode success preserves: ", ($pre === fastjson_last_error() ? "yes" : "no"), "\n";
?>
--EXPECT--
validate depth=0 last_error: 0
validate depth=-1 last_error: 0
decode depth=0 last_error: 0
decode depth=-1 last_error: 0
throw-mode success preserves: yes
