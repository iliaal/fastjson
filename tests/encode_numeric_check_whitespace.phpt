--TEST--
fastjson_encode: JSON_NUMERIC_CHECK honors leading-whitespace numeric strings (matches ext/json/is_numeric_string)
--EXTENSIONS--
fastjson
--INI--
serialize_precision=-1
--FILE--
<?php
/* is_numeric_string() skips leading whitespace, so " 123" is numeric.
 * The direct-write fast-reject must not treat a whitespace first byte as
 * non-numeric. Trailing whitespace is likewise accepted by
 * is_numeric_string on PHP 8. Each fastjson result must match ext/json. */
$cases = [" 123", "\t42", "  3.14", "\n7", "123 ", "42\t", "abc", " x", "", "0", " 0 "];
foreach ($cases as $c) {
    $fj = fastjson_encode($c, JSON_NUMERIC_CHECK);
    $ej = json_encode($c, JSON_NUMERIC_CHECK);
    printf("%-8s fj=%-10s match=%s\n", json_encode($c), $fj, $fj === $ej ? "yes" : "NO ($ej)");
}
?>
--EXPECT--
" 123"   fj=123        match=yes
"\t42"   fj=42         match=yes
"  3.14" fj=3.14       match=yes
"\n7"    fj=7          match=yes
"123 "   fj=123        match=yes
"42\t"   fj=42         match=yes
"abc"    fj="abc"      match=yes
" x"     fj=" x"       match=yes
""       fj=""         match=yes
"0"      fj=0          match=yes
" 0 "    fj=0          match=yes
