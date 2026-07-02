--TEST--
fastjson_encode: deep pretty-print indentation matches json_encode
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Nesting beyond 8 levels takes the memset indent fast path in
 * dw_emit_newline_indent (shallower levels stay on the append loop). Both
 * must produce byte-identical indentation to json_encode, so assert
 * parity across several deep shapes and pin the innermost indent width. */

$shapes = [];

$a = 1;
for ($i = 0; $i < 12; $i++) $a = [$a];
$shapes['nested-array-12'] = $a;

$o = ['v' => 1];
for ($i = 0; $i < 12; $i++) $o = ['n' => $o];
$shapes['nested-object-12'] = $o;

// Deep nesting with siblings at each level (commas + indent interplay).
$m = ['a' => 1, 'b' => 2];
for ($i = 0; $i < 10; $i++) $m = ['x' => $m, 'y' => [1, 2]];
$shapes['mixed-siblings-10'] = $m;

// Straddle the 8-level threshold (levels 7..10).
$b = 'leaf';
for ($i = 0; $i < 10; $i++) $b = [$b];
$shapes['straddle-threshold-10'] = $b;

foreach ($shapes as $label => $value) {
    $fj = fastjson_encode($value, JSON_PRETTY_PRINT);
    $ej = json_encode($value, JSON_PRETTY_PRINT);
    printf("%-24s %s\n", $label, $fj === $ej ? 'match' : "DIFF\n--fj--\n$fj\n--ej--\n$ej");
}

/* Innermost line of a 10-deep array indents 10 levels * 4 spaces = 40. */
$deep10 = 1;
for ($i = 0; $i < 10; $i++) $deep10 = [$deep10];
foreach (explode("\n", fastjson_encode($deep10, JSON_PRETTY_PRINT)) as $line) {
    if (trim($line) === '1') {
        echo "innermost indent: ", strlen($line) - strlen(ltrim($line, ' ')), " spaces\n";
        break;
    }
}
?>
--EXPECT--
nested-array-12          match
nested-object-12         match
mixed-siblings-10        match
straddle-threshold-10    match
innermost indent: 40 spaces
