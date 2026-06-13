--TEST--
fastjson_pointer_set: repeated edits on a large doc (ASAN free-path coverage)
--EXTENSIONS--
fastjson
--FILE--
<?php

// Build a sizable nested document and exercise the idoc/vdoc/mdoc/out
// allocation+free paths many times. Under ASAN this surfaces any leak or
// use-after-free across the mutable-doc juggling; under a normal build it
// just confirms the values round-trip.
$rows = [];
for ($i = 0; $i < 200; $i++) {
    $rows[] = ['id' => $i, 'name' => "user$i", 'tags' => ['a', 'b', 'c']];
}
$doc = fastjson_encode(['rows' => $rows, 'meta' => ['v' => 1]]);

$cur = $doc;
for ($i = 0; $i < 200; $i++) {
    $cur = fastjson_pointer_set($cur, "/rows/$i/name", "renamed$i");
    if ($cur === false) {
        echo "FAIL at $i: ", fastjson_last_error_msg(), "\n";
        break;
    }
}

// Set a value that is itself a deep structure, and replace meta wholesale.
$cur = fastjson_pointer_set($cur, '/meta', ['v' => 2, 'extra' => [1, [2, [3]]]]);

$back = fastjson_decode($cur, true);
echo $back['rows'][0]['name'], "\n";
echo $back['rows'][199]['name'], "\n";
echo $back['meta']['v'], "\n";
echo $back['meta']['extra'][1][1][0], "\n";
echo count($back['rows']), "\n";
?>
--EXPECT--
renamed0
renamed199
2
3
200
