--TEST--
fastjson_last_error_pos: byte offset + line/col for parse errors
--EXTENSIONS--
fastjson
--FILE--
<?php

// Single-line syntax error: offset of the offending byte, 1-based line/col.
fastjson_decode('{"a": bad}');
echo "pos=", fastjson_last_error_pos(), "\n";
$i = fastjson_last_error_info();
echo "line={$i['line']} col={$i['col']}\n";

// Multi-line input: line/col track the actual row/column.
$ml = "{\n  \"a\": 1,\n  \"b\": @\n}";
fastjson_decode($ml);
$i = fastjson_last_error_info();
echo "pos={$i['pos']} line={$i['line']} col={$i['col']}\n";

// Success resets the location to -1 / 0 / 0.
fastjson_decode('[1,2,3]');
echo "pos=", fastjson_last_error_pos(), "\n";

// fastjson_validate also records the position.
fastjson_validate('[1,2,');
echo "validate pos=", fastjson_last_error_pos(), "\n";

// The empty-input fast path has the same coordinates as decode.
fastjson_validate('');
$i = fastjson_last_error_info();
echo "empty pos={$i['pos']} line={$i['line']} col={$i['col']}\n";

// Encode errors carry no source offset.
fastjson_encode(fopen('php://memory', 'r'));
echo "encode pos=", fastjson_last_error_pos(), "\n";

// UTF-8 vs syntax classification is unchanged; a malformed byte still
// reports a position.
fastjson_decode("\"a\xff\"");
$i = fastjson_last_error_info();
echo "utf8 code=", ($i['code'] === JSON_ERROR_UTF8 ? 'UTF8' : 'OTHER'),
     " pos>=0=", ($i['pos'] >= 0 ? 'yes' : 'no'), "\n";
?>
--EXPECT--
pos=6
line=1 col=7
pos=19 line=3 col=8
pos=-1
validate pos=5
empty pos=0 line=1 col=1
encode pos=-1
utf8 code=UTF8 pos>=0=yes
