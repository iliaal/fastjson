--TEST--
FASTJSON_ERROR_* constants are registered and match ext/json JSON_ERROR_* int values
--EXTENSIONS--
fastjson
--FILE--
<?php

var_dump(defined('FASTJSON_ERROR_NONE'));
var_dump(defined('FASTJSON_ERROR_DEPTH'));
var_dump(defined('FASTJSON_ERROR_SYNTAX'));
var_dump(defined('FASTJSON_ERROR_UTF8'));

var_dump(FASTJSON_ERROR_NONE);
var_dump(FASTJSON_ERROR_DEPTH);
var_dump(FASTJSON_ERROR_SYNTAX);
var_dump(FASTJSON_ERROR_UTF8);

// Values are intentionally aligned with ext/json so callers can use
// either set of constants when both extensions are loaded.
var_dump(FASTJSON_ERROR_NONE === JSON_ERROR_NONE);
var_dump(FASTJSON_ERROR_DEPTH === JSON_ERROR_DEPTH);
var_dump(FASTJSON_ERROR_SYNTAX === JSON_ERROR_SYNTAX);
var_dump(FASTJSON_ERROR_UTF8 === JSON_ERROR_UTF8);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
int(0)
int(1)
int(4)
int(5)
bool(true)
bool(true)
bool(true)
bool(true)
