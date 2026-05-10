--TEST--
fastjson_decode() tests
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(fastjson_decode(""));
var_dump(fastjson_decode("", 1));
var_dump(fastjson_decode("", 0));
var_dump(fastjson_decode(".", 1));
var_dump(fastjson_decode(".", 0));
var_dump(fastjson_decode("<?>"));
var_dump(fastjson_decode(";"));
var_dump(fastjson_decode("руссиш"));
var_dump(fastjson_decode("blah"));
var_dump(fastjson_decode(NULL));
var_dump(fastjson_decode('{ "test": { "foo": "bar" } }'));
var_dump(fastjson_decode('{ "test": { "foo": "" } }'));
var_dump(fastjson_decode('{ "": { "foo": "" } }'));
var_dump(fastjson_decode('{ "": { "": "" } }'));
var_dump(fastjson_decode('{ "": { "": "" }'));
var_dump(fastjson_decode('{ "": "": "" } }'));

?>
--EXPECTF--
NULL
NULL
NULL
NULL
NULL
NULL
NULL
NULL
NULL

Deprecated: fastjson_decode(): Passing null to parameter #1 ($json) of type string is deprecated in %s on line %d
NULL
object(stdClass)#%d (1) {
  ["test"]=>
  object(stdClass)#%d (1) {
    ["foo"]=>
    string(3) "bar"
  }
}
object(stdClass)#%d (1) {
  ["test"]=>
  object(stdClass)#%d (1) {
    ["foo"]=>
    string(0) ""
  }
}
object(stdClass)#%d (1) {
  [""]=>
  object(stdClass)#%d (1) {
    ["foo"]=>
    string(0) ""
  }
}
object(stdClass)#%d (1) {
  [""]=>
  object(stdClass)#%d (1) {
    [""]=>
    string(0) ""
  }
}
NULL
NULL
