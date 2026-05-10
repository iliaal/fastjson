--TEST--
fastjson_encode: round-trip + parity vs ext/json on common cases
--EXTENSIONS--
fastjson
json
--FILE--
<?php

// Round-trip: every encodable value reconstructs exactly via assoc decode.
$cases = [
    null,
    true,
    false,
    0,
    -1,
    1234567890,
    3.14,
    -0.5,
    "hello",
    "",
    "héllo \xf0\x9f\x91\x8b",  // emoji
    [],
    [1, 2, 3],
    ["a" => 1, "b" => 2],
    [[1, 2], [3, 4]],
    ["k" => null, "list" => [1, "two", true, null]],
];

foreach ($cases as $i => $value) {
    $encoded = fastjson_encode($value);
    if ($encoded === false) {
        echo "case $i: encode FAILED\n";
        continue;
    }
    $decoded = fastjson_decode($encoded, true);
    if ($decoded !== $value) {
        echo "case $i: round-trip mismatch\n";
        var_dump($value, $decoded);
    }
}
echo "round-trip OK\n";

echo "---\n";

// Parity vs ext/json: byte-equality on cases without unicode-escape
// hex (todos/002 documents the case divergence). Use UNESCAPED_UNICODE
// for cases involving non-ASCII.
$flags_cases = [
    [["a" => 1, "b" => [2, 3]], 0],
    [[1, 2, 3], 0],
    [null, 0],
    ["plain ascii", 0],
    [["url" => "/a/b"], 0],                                          // slash escapes
    [["url" => "/a/b"], JSON_UNESCAPED_SLASHES],
    [["greet" => "héllo"], JSON_UNESCAPED_UNICODE],
    [["pretty" => [1, 2]], JSON_PRETTY_PRINT],
];

foreach ($flags_cases as $i => [$value, $flags]) {
    $f = fastjson_encode($value, $flags);
    $j = json_encode($value, $flags);
    if ($f !== $j) {
        echo "parity case $i diverged\n";
        var_dump(['fastjson' => $f, 'json' => $j]);
    }
}
echo "parity OK\n";
?>
--EXPECT--
round-trip OK
---
parity OK
