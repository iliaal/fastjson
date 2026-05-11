--TEST--
fastjson encode/decode honor JSON_INVALID_UTF8_IGNORE / SUBSTITUTE (matches ext/json)
--EXTENSIONS--
fastjson
--FILE--
<?php

// Encode side. ext/json strips invalid bytes under IGNORE, replaces
// them with U+FFFD under SUBSTITUTE, and gives IGNORE precedence
// when both flags are set.
$inputs = [
    "ascii"        => "abc",
    "valid 2-byte" => "héllo",
    "lone cont"    => "a\xFFb",
    "truncated"    => "a\xC3",
    "overlong"     => "a\xC0\x80b",
    "trailing"     => "ab\xFF",
];
$flagsets = [
    "IGNORE"      => JSON_INVALID_UTF8_IGNORE,
    "SUBSTITUTE"  => JSON_INVALID_UTF8_SUBSTITUTE,
    "BOTH"        => JSON_INVALID_UTF8_IGNORE | JSON_INVALID_UTF8_SUBSTITUTE,
];

$miss = 0;
foreach ($inputs as $label => $s) {
    foreach ($flagsets as $fname => $f) {
        $fj = fastjson_encode([$s], $f);
        $rj = json_encode([$s], $f);
        if ($fj !== $rj) {
            $miss++;
            printf("ENC %-13s %-10s fj=%s rj=%s\n", $label, $fname, var_export($fj, true), var_export($rj, true));
        }
    }
}

// Decode side: same semantics applied to decoded string values and
// object keys.
$json_inputs = [
    "string val" => '["a' . "\xFF" . 'b"]',
    "key"        => '{"a' . "\xFF" . 'b":1}',
    "trunc tail" => '["x' . "\xC3" . '"]',
    "overlong"   => '["a' . "\xC0\x80" . 'b"]',
    "nested"     => '{"k":["x' . "\xFF" . 'y"]}',
];
foreach ($json_inputs as $label => $j) {
    foreach ($flagsets as $fname => $f) {
        $fj = fastjson_decode($j, true, 512, $f);
        $rj = json_decode($j, true, 512, $f);
        if ($fj != $rj) {
            $miss++;
            printf("DEC %-13s %-10s fj=%s rj=%s\n", $label, $fname, json_encode($fj), json_encode($rj));
        }
    }
}

// Without the flag, both functions still fail on invalid UTF-8.
var_dump(fastjson_encode(["a\xFFb"]));
var_dump(fastjson_last_error() === FASTJSON_ERROR_UTF8);
var_dump(fastjson_decode('["a' . "\xFF" . 'b"]'));
var_dump(fastjson_last_error() === FASTJSON_ERROR_UTF8);

echo "miss=$miss\n";
?>
--EXPECT--
bool(false)
bool(true)
NULL
bool(true)
miss=0
