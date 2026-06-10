--TEST--
fastjson_encode: enums serialize as their backing value (ext/json parity)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Regression: dw_emit_object had no ZEND_ACC_ENUM branch, so enum cases
 * fell through to the property-table walker and serialized as
 * {"name":...,"value":...} instead of the backing value. ext/json
 * dispatches enums (after the JsonSerializable check) to emit the
 * backing value, or fail with JSON_ERROR_NON_BACKED_ENUM. */

enum Suit: string { case Hearts = "H"; case Spades = "S"; }
enum Bits: int { case Two = 2; case Eight = 8; }
enum Plain { case A; case B; }

enum Mood: string implements JsonSerializable {
    case Happy = "happy";
    public function jsonSerialize(): mixed { return "MOOD:" . $this->value; }
}

// Backed enums match json_encode byte-for-byte.
var_dump(fastjson_encode(Suit::Hearts) === json_encode(Suit::Hearts));
var_dump(fastjson_encode(Bits::Eight) === json_encode(Bits::Eight));
var_dump(fastjson_encode([Suit::Hearts, Bits::Two]) === json_encode([Suit::Hearts, Bits::Two]));

// JsonSerializable wins over the backing value (dispatch order).
var_dump(fastjson_encode(Mood::Happy) === json_encode(Mood::Happy));
echo fastjson_encode(Mood::Happy), "\n";

// Non-backed enum: error, no output.
var_dump(fastjson_encode(Plain::A));
var_dump(fastjson_last_error() === FASTJSON_ERROR_NON_BACKED_ENUM);

// Under PARTIAL_OUTPUT_ON_ERROR a non-backed enum substitutes 0,
// matching php_json_encode_serializable_enum.
echo fastjson_encode([Plain::A], JSON_PARTIAL_OUTPUT_ON_ERROR), "\n";

// THROW_ON_ERROR raises for the non-backed enum.
try {
    fastjson_encode(Plain::B, JSON_THROW_ON_ERROR);
    echo "no exception\n";
} catch (\JsonException $e) {
    echo "threw\n";
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
"MOOD:happy"
bool(false)
bool(true)
[0]
threw
