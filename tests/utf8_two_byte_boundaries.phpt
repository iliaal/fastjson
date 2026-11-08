--TEST--
fastjson UTF-8 handling preserves every two-byte code point and rejects invalid boundaries
--EXTENSIONS--
fastjson
--FILE--
<?php
$flags = [
    JSON_INVALID_UTF8_IGNORE,
    JSON_INVALID_UTF8_SUBSTITUTE,
    JSON_INVALID_UTF8_IGNORE | JSON_INVALID_UTF8_SUBSTITUTE,
];

function checkTwoByte(string $input, string $label): void {
    global $flags;
    foreach ($flags as $flag) {
        // Check the fast path without a sanitizer-forcing suffix as well.
        $expected = json_encode($input, $flag);
        if (fastjson_pointer_set('null', '', $input, 512, $flag) !== $expected
            || fastjson_last_error() !== JSON_ERROR_NONE) {
            echo "clean pointer mismatch: $label/$flag\n";
        }
        // The invalid suffix forces the encoder through its sanitizer even
        // when the two-byte sequence itself is valid.
        $inputWithError = $input . "\xFF";
        $expected = json_encode($inputWithError, $flag);
        if (fastjson_encode($inputWithError, $flag) !== $expected
            || fastjson_last_error() !== json_last_error()) {
            echo "encode mismatch: $label/$flag\n";
        }
        // String replacement checks the separate well-formedness helper.
        if (fastjson_pointer_set('null', '', $inputWithError, 512, $flag) !== $expected
            || fastjson_last_error() !== JSON_ERROR_NONE) {
            echo "pointer mismatch: $label/$flag\n";
        }
        // Inputs contain no JSON metacharacters, so retain their raw bytes.
        $json = '"' . $inputWithError . '"';
        $expected = json_decode($json, true, 512, $flag);
        if (fastjson_decode($json, true, 512, $flag) !== $expected
            || fastjson_last_error() !== json_last_error()) {
            echo "decode mismatch: $label/$flag\n";
        }
    }
}

$count = 0;
for ($lead = 0xC2; $lead <= 0xDF; $lead++) {
    for ($trail = 0x80; $trail <= 0xBF; $trail++) {
        checkTwoByte(chr($lead) . chr($trail), "$lead/$trail");
        $count++;
    }
}
echo "valid sequences: $count\n";

foreach ([
    "\xC0\x80", "\xC1\xBF", // Overlong leads stay invalid.
    "\xC2", "\xDF",         // Truncated sequences.
    "\xC2A", "\xDF\x7F",   // ASCII is not a continuation byte.
    "\xC2\xC0", "\xDF\xFF", // Invalid high-byte continuations.
    "\x80", "\xBF",         // Lone continuation bytes.
    // The 3-/4-byte range checks must still reject these complete sequences.
    "\xE0\x80\x80", "\xED\xA0\x80",
    "\xF0\x80\x80\x80", "\xF4\x90\x80\x80",
] as $input) {
    checkTwoByte($input, bin2hex($input));
    if (fastjson_encode($input) !== false
        || fastjson_last_error() !== JSON_ERROR_UTF8) {
        echo "invalid sequence accepted: ", bin2hex($input), "\n";
    }
}
echo "boundaries checked\n";
?>
--EXPECT--
valid sequences: 1920
boundaries checked
