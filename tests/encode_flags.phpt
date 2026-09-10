--TEST--
fastjson_encode: PRETTY_PRINT, UNESCAPED_SLASHES, UNESCAPED_UNICODE
--EXTENSIONS--
fastjson
--FILE--
<?php

echo fastjson_encode(["a" => [1, 2], "b" => 3], JSON_PRETTY_PRINT), "\n";

echo "---\n";

var_dump(fastjson_encode("a/b/c"));

var_dump(fastjson_encode("a/b/c", JSON_UNESCAPED_SLASHES));

echo "---\n";

// Vendor patch P-001 aligns escape hex casing with ext/json.
$encoded = fastjson_encode("héllo");
var_dump(strpos($encoded, '\u00') !== false);
var_dump(fastjson_decode($encoded) === "héllo");

var_dump(fastjson_encode("héllo", JSON_UNESCAPED_UNICODE));

echo "---\n";

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
