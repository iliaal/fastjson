--TEST--
fastjson_validate: $flags accepts only JSON_INVALID_UTF8_IGNORE; other bits raise ValueError
--EXTENSIONS--
fastjson
--FILE--
<?php

// JSON_INVALID_UTF8_IGNORE: invalid UTF-8 inside a string validates true.
$bad = "\"a\xFFb\"";
var_dump(fastjson_validate($bad));                          // false, no flag
var_dump(fastjson_validate($bad, 512, JSON_INVALID_UTF8_IGNORE));

// Any other flag bit must raise ValueError matching ext/json's contract.
foreach ([JSON_THROW_ON_ERROR, JSON_OBJECT_AS_ARRAY, JSON_BIGINT_AS_STRING] as $flag) {
    try {
        fastjson_validate("{}", 512, $flag);
        echo "no throw for flag $flag\n";
    } catch (ValueError $e) {
        echo "ValueError: ", $e->getMessage(), "\n";
    }
}

// Combination including the allowed bit but with extras: still rejected.
try {
    fastjson_validate("{}", 512, JSON_INVALID_UTF8_IGNORE | JSON_THROW_ON_ERROR);
    echo "no throw for combined\n";
} catch (ValueError $e) {
    echo "ValueError: combined\n";
}

// flags=0 stays valid.
var_dump(fastjson_validate("[1,2]"));
?>
--EXPECT--
bool(false)
bool(true)
ValueError: fastjson_validate(): Argument #3 ($flags) must be a valid flag (allowed flags: JSON_INVALID_UTF8_IGNORE)
ValueError: fastjson_validate(): Argument #3 ($flags) must be a valid flag (allowed flags: JSON_INVALID_UTF8_IGNORE)
ValueError: fastjson_validate(): Argument #3 ($flags) must be a valid flag (allowed flags: JSON_INVALID_UTF8_IGNORE)
ValueError: combined
bool(true)
