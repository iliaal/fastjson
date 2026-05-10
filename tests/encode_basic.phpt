--TEST--
fastjson_encode: scalars, lists, assoc arrays, mixed nesting
--EXTENSIONS--
fastjson
--FILE--
<?php

// Scalars.
var_dump(fastjson_encode(null));
var_dump(fastjson_encode(true));
var_dump(fastjson_encode(false));
var_dump(fastjson_encode(0));
var_dump(fastjson_encode(-7));
var_dump(fastjson_encode(3.14));
var_dump(fastjson_encode("hello"));
var_dump(fastjson_encode(""));

echo "---\n";

// Lists -> JSON arrays.
var_dump(fastjson_encode([]));
var_dump(fastjson_encode([1, 2, 3]));
var_dump(fastjson_encode(["a", true, null, 1.5]));
var_dump(fastjson_encode([[1, 2], [3, 4]]));

echo "---\n";

// Assoc arrays -> JSON objects.
var_dump(fastjson_encode(["a" => 1, "b" => 2]));
var_dump(fastjson_encode(["nested" => ["k" => "v"]]));

echo "---\n";

// Last-error reset on success.
fastjson_encode(INF);  // populate failure state
var_dump(fastjson_last_error() !== FASTJSON_ERROR_NONE);
fastjson_encode([1, 2, 3]);
var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);
?>
--EXPECT--
string(4) "null"
string(4) "true"
string(5) "false"
string(1) "0"
string(2) "-7"
string(4) "3.14"
string(7) ""hello""
string(2) """"
---
string(2) "[]"
string(7) "[1,2,3]"
string(19) "["a",true,null,1.5]"
string(13) "[[1,2],[3,4]]"
---
string(13) "{"a":1,"b":2}"
string(20) "{"nested":{"k":"v"}}"
---
bool(true)
bool(true)
