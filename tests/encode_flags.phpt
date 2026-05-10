--TEST--
fastjson_encode: PRETTY_PRINT, UNESCAPED_SLASHES, UNESCAPED_UNICODE
--EXTENSIONS--
fastjson
--FILE--
<?php

// PRETTY_PRINT: 4-space indent, newlines.
echo fastjson_encode(["a" => [1, 2], "b" => 3], JSON_PRETTY_PRINT), "\n";

echo "---\n";

// Default: forward slashes are escaped (matches ext/json default).
var_dump(fastjson_encode("a/b/c"));

// UNESCAPED_SLASHES: raw slashes.
var_dump(fastjson_encode("a/b/c", JSON_UNESCAPED_SLASHES));

echo "---\n";

// Default: non-ASCII as \uXXXX escapes (matches ext/json default).
// Hex digit case differs from ext/json (uppercase here, lowercase
// upstream); see todos/002. Round-trip via decode for correctness.
$encoded = fastjson_encode("héllo");
var_dump(strpos($encoded, '\u00') !== false);   // some \u escape present
var_dump(fastjson_decode($encoded) === "héllo"); // round-trips

// UNESCAPED_UNICODE: raw UTF-8.
var_dump(fastjson_encode("héllo", JSON_UNESCAPED_UNICODE));

echo "---\n";

// Combined flags.
$mixed = ["url" => "https://example.com/é", "n" => 1];
$out = fastjson_encode($mixed,
    JSON_UNESCAPED_SLASHES | JSON_UNESCAPED_UNICODE | JSON_PRETTY_PRINT);
echo $out, "\n";
?>
--EXPECT--
{
    "a": [
        1,
        2
    ],
    "b": 3
}
---
string(9) ""a\/b\/c""
string(7) ""a/b/c""
---
bool(true)
bool(true)
string(8) ""héllo""
---
{
    "url": "https://example.com/é",
    "n": 1
}
