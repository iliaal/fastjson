--TEST--
fastjson_validate() - General usage
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(
  fastjson_validate(""),
  fastjson_validate("."),
  fastjson_validate("<?>"),
  fastjson_validate(";"),
  fastjson_validate("руссиш"),
  fastjson_validate("blah"),
  fastjson_validate('{ "": "": "" } }'),
  fastjson_validate('{ "": { "": "" }'),
  fastjson_validate('{ "test": {} "foo": "bar" }, "test2": {"foo" : "bar" }, "test2": {"foo" : "bar" } }'),

  fastjson_validate('{ "test": { "foo": "bar" } }'),
  fastjson_validate('{ "test": { "foo": "" } }'),
  fastjson_validate('{ "": { "foo": "" } }'),
  fastjson_validate('{ "": { "": "" } }'),
  fastjson_validate('{ "test": {"foo": "bar"}, "test2": {"foo" : "bar" }, "test2": {"foo" : "bar" } }'),
  fastjson_validate('{ "test": {"foo": "bar"}, "test2": {"foo" : "bar" }, "test3": {"foo" : "bar" } }'),
);

?>
--EXPECT--
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
