--TEST--
fastjson_last_error / fastjson_last_error_msg: track most recent call, reset on success
--EXTENSIONS--
fastjson
--FILE--
<?php

// Initial state: no error recorded.
var_dump(fastjson_last_error());
var_dump(fastjson_last_error_msg());

echo "---\n";

// Failure populates code and message.
var_dump(fastjson_validate('not json'));
$code = fastjson_last_error();
$msg = fastjson_last_error_msg();
var_dump($code === FASTJSON_ERROR_SYNTAX);
var_dump($msg);

echo "---\n";

// Success resets the error state.
var_dump(fastjson_validate('{"ok": true}'));
var_dump(fastjson_last_error());
var_dump(fastjson_last_error_msg());

echo "---\n";

// Subsequent failure repopulates with that call's code.
var_dump(fastjson_validate('{'));
var_dump(fastjson_last_error() === FASTJSON_ERROR_SYNTAX);
var_dump(fastjson_last_error_msg());
?>
--EXPECT--
int(0)
string(8) "No error"
---
bool(false)
bool(true)
string(32) "invalid literal, expected 'null'"
---
bool(true)
int(0)
string(8) "No error"
---
bool(false)
bool(true)
string(22) "unexpected end of data"
