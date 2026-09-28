--TEST--
chunked encode does not split a UTF-8 sequence on an 8192-byte boundary
--EXTENSIONS--
fastjson
--FILE--
<?php

function check(string $value, int $flags = 0): void
{
    var_dump(fastjson_encode($value, $flags) === json_encode($value, $flags));
}

/* Early quote forces the chunked writer. The non-ASCII byte sits on the
 * first 8192-byte cut, before the late-escape tail. */
$two = '"' . str_repeat('a', 8189) . "é" . str_repeat('b', 2000);
$three = '"' . str_repeat('a', 8190) . "\u{4E00}" . str_repeat('b', 2000);
$four = '"' . str_repeat('a', 8190) . "\u{1F600}" . str_repeat('b', 2000);
$second = '"' . str_repeat('a', 16381) . "é" . str_repeat('b', 100);

check($two);
check($three);
check($four);
check($second);
check($two, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
check($two, JSON_INVALID_UTF8_SUBSTITUTE);
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
