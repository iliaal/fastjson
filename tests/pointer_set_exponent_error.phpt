--TEST--
fastjson_pointer_set: an exponent without digits reports the decode error and position
--EXTENSIONS--
fastjson
--FILE--
<?php

foreach (['[1e]', '[-1.5e+]', '{"a":1e,"b":2}', '[1E-x]'] as $json) {
    fastjson_decode($json);
    $decode = [fastjson_last_error(), fastjson_last_error_msg(), fastjson_last_error_pos()];
    var_dump(fastjson_pointer_set($json, '/0', 5));
    $set = [fastjson_last_error(), fastjson_last_error_msg(), fastjson_last_error_pos()];
    echo $json, ' ', json_encode($set), ' ', $set === $decode ? 'same' : 'differs', "\n";
}
?>
--EXPECT--
bool(false)
[1e] [4,"no digit after exponent sign",3] same
bool(false)
[-1.5e+] [4,"no digit after exponent sign",7] same
bool(false)
{"a":1e,"b":2} [4,"no digit after exponent sign",7] same
bool(false)
[1E-x] [4,"no digit after exponent sign",4] same
