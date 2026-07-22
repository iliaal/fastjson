--TEST--
fastjson_last_error_info reports exact Unicode and BOM coordinates
--EXTENSIONS--
fastjson
--FILE--
<?php

fastjson_decode("{\"é\":1,@}");
$info = fastjson_last_error_info();
echo "unicode pos={$info['pos']} line={$info['line']} col={$info['col']}\n";

fastjson_decode("{\n  \"é\": 1,\n  \"b\": @\n}");
$info = fastjson_last_error_info();
echo "multiline pos={$info['pos']} line={$info['line']} col={$info['col']}\n";

fastjson_decode("\xEF\xBB\xBF{\"é\":1,@}");
$info = fastjson_last_error_info();
echo "strict-bom pos={$info['pos']} line={$info['line']} col={$info['col']}\n";

fastjson_decode(
    "\xEF\xBB\xBF{\"é\":1,@}",
    null,
    512,
    FASTJSON_DECODE_RELAXED,
);
$info = fastjson_last_error_info();
echo "relaxed-bom pos={$info['pos']} line={$info['line']} col={$info['col']}\n";
?>
--EXPECT--
unicode pos=8 line=1 col=8
multiline pos=20 line=3 col=8
strict-bom pos=0 line=1 col=1
relaxed-bom pos=11 line=1 col=8
