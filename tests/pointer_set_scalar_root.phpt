--TEST--
fastjson_pointer_set: direct scalar nodes and root replacement
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Scalar replacement values (null/bool/int/string) build a yyjson node
 * directly instead of round-tripping through the encoder, and an empty
 * pointer ("") replaces the whole document without copying the base doc.
 * Cover each scalar type at a nested path, at the root, and inside an
 * array element, plus the boundary integers. */

$doc = '{"a":{"b":1},"list":[10,20]}';

// Direct scalar at a nested path.
echo fastjson_pointer_set($doc, '/a/b', null), "\n";
echo fastjson_pointer_set($doc, '/a/b', true), "\n";
echo fastjson_pointer_set($doc, '/a/b', false), "\n";
echo fastjson_pointer_set($doc, '/a/b', 42), "\n";
echo fastjson_pointer_set($doc, '/a/b', PHP_INT_MAX), "\n";
echo fastjson_pointer_set($doc, '/a/b', PHP_INT_MIN), "\n";
echo fastjson_pointer_set($doc, '/a/b', 'text'), "\n";

// Scalar into an array element.
echo fastjson_pointer_set($doc, '/list/0', null), "\n";

// Root replacement ("") with each scalar and a non-scalar.
echo fastjson_pointer_set($doc, '', null), "\n";
echo fastjson_pointer_set($doc, '', true), "\n";
echo fastjson_pointer_set($doc, '', 7), "\n";
echo fastjson_pointer_set($doc, '', 'top'), "\n";
echo fastjson_pointer_set($doc, '', [1, 2, ['k' => 'v']]), "\n";

// Scalar string still honors output-formatting flags on the write.
echo fastjson_pointer_set($doc, '/a/b', 'a/b'), "\n";
echo fastjson_pointer_set($doc, '/a/b', 'a/b', 512, JSON_UNESCAPED_SLASHES), "\n";

// Root replacement parses the base document for validity, but doesn't walk or
// re-emit it. A stack-safe depth cap for the discarded target is unnecessary.
$deep = str_repeat('{"a":', 1100) . '1' . str_repeat('}', 1100);
echo fastjson_pointer_set($deep, '', 1, 100000), "\n";

var_dump(fastjson_last_error());
?>
--EXPECT--
{"a":{"b":null},"list":[10,20]}
{"a":{"b":true},"list":[10,20]}
{"a":{"b":false},"list":[10,20]}
{"a":{"b":42},"list":[10,20]}
{"a":{"b":9223372036854775807},"list":[10,20]}
{"a":{"b":-9223372036854775808},"list":[10,20]}
{"a":{"b":"text"},"list":[10,20]}
{"a":{"b":1},"list":[null,20]}
null
true
7
"top"
[1,2,{"k":"v"}]
{"a":{"b":"a\/b"},"list":[10,20]}
{"a":{"b":"a/b"},"list":[10,20]}
1
int(0)
