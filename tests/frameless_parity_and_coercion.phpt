--TEST--
fastjson frameless call parity, coercion, and error handling
--EXTENSIONS--
fastjson
--FILE--
<?php
class StrBox {
    public function __construct(private string $s) {}
    public function __toString(): string { return $this->s; }
}
class ThrowingStr {
    public function __toString(): string { throw new RuntimeException('boom'); }
}

// 0-arg, 1-arg, 2-arg, 3-arg direct calls (frameless on PHP 8.4+)
var_dump(is_string(fastjson_version()));
var_dump(fastjson_decode('{"a":1}')->a);
var_dump(fastjson_decode('{"a":1}', true));
var_dump(fastjson_decode('{"a":1}', false)->a);
var_dump(fastjson_decode('{"a":1}', null)->a);
var_dump(fastjson_validate('{"a":1}'));
var_dump(fastjson_validate('[[1]]', 2), fastjson_last_error() === FASTJSON_ERROR_DEPTH);
var_dump(fastjson_validate("[\"a\xFF\"]", 512, JSON_INVALID_UTF8_IGNORE));
var_dump(fastjson_pointer_get('{"a":{"b":7}}', '/a/b'));
var_dump(fastjson_pointer_get('{"a":{"b":7}}', '/a', true));
var_dump(fastjson_pointer_exists('{"a":null}', '/a'));
var_dump(fastjson_pointer_exists('{"a":null}', ''));
var_dump(fastjson_pointer_exists('{bad', ''));
var_dump(fastjson_pointer_exists('{"a":1,}', '/a', FASTJSON_DECODE_RELAXED));
var_dump(fastjson_merge_patch('{"a":1,"b":2}', '{"b":null,"c":3}', true));
var_dump(fastjson_last_error(), fastjson_last_error_msg(), fastjson_last_error_pos());
var_dump(count(fastjson_last_error_info()));

// Stringable coercion and throwing __toString (must not leak)
var_dump(fastjson_decode(new StrBox('{"k":9}'), true));
var_dump(fastjson_pointer_get(new StrBox('{"k":9}'), new StrBox('/k'), true));
var_dump(fastjson_pointer_exists(new StrBox('{"k":9}'), new StrBox('/k')));
var_dump(fastjson_validate(new StrBox('[1,2]')));
try {
    fastjson_decode(new ThrowingStr(), true);
} catch (RuntimeException $e) {
    echo "caught1:" . $e->getMessage() . "\n";
}
try {
    fastjson_pointer_get(new StrBox('{"k":1}'), new ThrowingStr(), true);
} catch (RuntimeException $e) {
    echo "caught2:" . $e->getMessage() . "\n";
}

// TypeError & ValueError parity
try {
    fastjson_decode('{"a":1}', []);
} catch (TypeError $e) {
    echo "typeerr:" . (str_contains($e->getMessage(), 'fastjson_decode') ? 'ok' : $e->getMessage()) . "\n";
}
try {
    fastjson_validate('{}', 0);
} catch (ValueError $e) {
    echo "valerr:" . (str_contains($e->getMessage(), 'fastjson_validate') ? 'ok' : $e->getMessage()) . "\n";
}
?>
--EXPECT--
bool(true)
int(1)
array(1) {
  ["a"]=>
  int(1)
}
int(1)
int(1)
bool(true)
bool(false)
bool(true)
bool(true)
int(7)
array(1) {
  ["b"]=>
  int(7)
}
bool(true)
bool(true)
bool(false)
bool(true)
array(2) {
  ["a"]=>
  int(1)
  ["c"]=>
  int(3)
}
int(0)
string(8) "No error"
int(-1)
int(5)
array(1) {
  ["k"]=>
  int(9)
}
int(9)
bool(true)
bool(true)
caught1:boom
caught2:boom
typeerr:ok
valerr:ok
