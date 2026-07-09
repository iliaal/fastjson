--TEST--
fastjson_pointer_set: the pointer path counts against the replacement depth budget
--EXTENSIONS--
fastjson
--FILE--
<?php

/* The replacement lands N containers deep in the output (N = pointer
 * segments), so its own nesting budget is what remains of $depth after
 * the path. Passing the full $depth let pointer_set emit a document
 * deeper than $depth -- one fastjson_decode(..., $depth) then rejects.
 * The output must always round-trip at the same $depth. */

// "/a" is one segment; [1] under it makes {"a":[1]} (nesting 2), which
// does not decode at depth 2. pointer_set must reject rather than emit it.
var_dump(fastjson_pointer_set('{}', '/a', [1], 2));
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);

// Same edit fits at depth 3 and round-trips at depth 3.
$out = fastjson_pointer_set('{}', '/a', [1], 3);
var_dump($out);
var_dump(fastjson_decode($out, true, 3));

// A scalar replacement adds no nesting, so it is allowed at depth 2.
var_dump(fastjson_pointer_set('{}', '/a', 5, 2));

// Root replacement (no path) keeps the full budget for the value.
var_dump(fastjson_pointer_set('{}', '', [[1]], 3));
?>
--EXPECT--
bool(false)
bool(true)
string(9) "{"a":[1]}"
array(1) {
  ["a"]=>
  array(1) {
    [0]=>
    int(1)
  }
}
string(7) "{"a":5}"
string(5) "[[1]]"
