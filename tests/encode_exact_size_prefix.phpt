--TEST--
fastjson exact-size string writer counts ASCII prefixes before escapes and invalid UTF-8
--EXTENSIONS--
fastjson
--INI--
memory_limit=256M
--FILE--
<?php

// At 8 MiB the large-string writer measures escaped output exactly.
// Vary the prefix alignment so the first special byte can also occur
// inside the final eight-byte scan word rather than only in its tail.
$cases = [
    ['"', 0],
    ['\\', 0],
    ["\x01", 0],
    ['/', 0],
    ['/', JSON_UNESCAPED_SLASHES],
    ["é", 0],
    ["\u{1F600}", 0],
    ["\u{1F600}", JSON_UNESCAPED_UNICODE],
    ["\xFF", 0],
    ["\xF0\x9F", 0],
    ["\xFF", JSON_INVALID_UTF8_IGNORE],
    ["\xFF", JSON_INVALID_UTF8_SUBSTITUTE],
];
$checks = 0;
foreach ([0, 1, 7] as $offset) {
    $prefix = str_repeat('a', 8 * 1024 * 1024 + $offset);
    foreach ($cases as [$tail, $flags]) {
        $value = $prefix . $tail;
        $expected = json_encode($value, $flags);
        $error = json_last_error();
        if (fastjson_encode($value, $flags) !== $expected
                || fastjson_last_error() !== $error) {
            echo "encode mismatch at offset $offset, tail ", bin2hex($tail), "\n";
        }
        $checks++;
    }
}
// An earlier escape returns a word-aligned prefix without refining the
// final word byte by byte; the remaining clean suffix still gets counted.
$value = str_repeat('a', 4 * 1024 * 1024 + 3) . '"'
    . str_repeat('b', 4 * 1024 * 1024);
var_dump(fastjson_encode($value) === json_encode($value));
unset($value, $expected);

// The immutable pointer writer uses the same exact-size helper for
// untouched strings in the original document, not just replacements.
$document = json_encode(['blob' => $prefix . "é\"", 'x' => 1]);
$expected = json_encode(['blob' => $prefix . "é\"", 'x' => 2]);
var_dump(fastjson_pointer_set($document, '/x', 2) === $expected);
echo "$checks encode cases\n";
?>
--EXPECT--
bool(true)
bool(true)
36 encode cases
