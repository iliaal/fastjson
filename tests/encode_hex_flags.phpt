--TEST--
fastjson_encode: JSON_HEX_TAG / HEX_AMP / HEX_APOS / HEX_QUOT substitutions
--EXTENSIONS--
fastjson
--FILE--
<?php

// Each input round-tripped through fastjson then json_decode must equal
// the input, and the output must byte-match json_encode().

$cases = [
    "<foo>",
    "'bar'",
    '"baz"',
    "&blong&",
    "no-special",
    // Trailing backslash: the second '\\' of yyjson's "\\\\" output sits
    // right before the closing '"' of the string. A context-blind scan
    // misreads that as a `\"` escape and substitutes " over the
    // close quote, producing unterminated JSON.
    "trailing\\",
    "\\",
    "\\\\",
    // Backslash + quote: the encoded form has a '\"' escape that DOES
    // legitimately need HEX_QUOT substitution.
    "\\\"",
    "embedded\\\"in middle",
    // Mixed everything.
    "<a>&'\\\"</a>",
];

$flags = [
    "TAG"  => JSON_HEX_TAG,
    "APOS" => JSON_HEX_APOS,
    "QUOT" => JSON_HEX_QUOT,
    "AMP"  => JSON_HEX_AMP,
    "ALL"  => JSON_HEX_TAG | JSON_HEX_APOS | JSON_HEX_QUOT | JSON_HEX_AMP,
];

$diverge = 0;
$broken = 0;
foreach ($cases as $in) {
    foreach ($flags as $name => $f) {
        $fj = fastjson_encode($in, $f);
        $rj = json_encode($in, $f);
        if ($fj !== $rj) {
            $diverge++;
            echo "DIVERGE input=", var_export($in, true), " flag=$name fj=", var_export($fj, true), " rj=", var_export($rj, true), "\n";
        }
        if ($fj !== false && json_decode($fj) !== $in) {
            $broken++;
            echo "BROKEN  input=", var_export($in, true), " flag=$name fj=", var_export($fj, true), "\n";
        }
    }
}
echo "diverge=$diverge broken=$broken\n";
?>
--EXPECT--
diverge=0 broken=0
