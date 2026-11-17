--TEST--
Malformed escapes report JSON_ERROR_SYNTAX; unpaired surrogates keep JSON_ERROR_UTF8
--EXTENSIONS--
fastjson
--FILE--
<?php
// [json, expected fastjson code]. ext/json reports SYNTAX for every
// malformed escape; fastjson keeps its documented UTF8 code for an unpaired
// surrogate (ext/json: UTF16) and a raw control character (ext/json: CTRL_CHAR).
$cases = [
    ['"\\x"', JSON_ERROR_SYNTAX],
    ["\"\\\xff\"", JSON_ERROR_SYNTAX],
    ['"\\u12"', JSON_ERROR_SYNTAX],
    ['"\\u12zz"', JSON_ERROR_SYNTAX],
    ['"\\u00G0"', JSON_ERROR_SYNTAX],
    ['"\\u00!0"', JSON_ERROR_SYNTAX],
    ['"\\u00 0"', JSON_ERROR_SYNTAX],
    ['"\\xd800"', JSON_ERROR_SYNTAX],
    ['{"\\q":1}', JSON_ERROR_SYNTAX],
    ['["ok", "\\a"]', JSON_ERROR_SYNTAX],
    ['"\\ud800"', JSON_ERROR_UTF8],
    ['"\\uDBFF\\u0041"', JSON_ERROR_UTF8],
    ['"\\udc00x"', JSON_ERROR_UTF8],
    ["\"\x01\"", JSON_ERROR_UTF8],
];
$fail = 0;
foreach ($cases as [$json, $want]) {
    foreach ([0, JSON_INVALID_UTF8_IGNORE, JSON_INVALID_UTF8_SUBSTITUTE] as $f) {
        $got = [];
        fastjson_decode($json, true, 512, $f);
        $got['decode'] = fastjson_last_error();
        if ($f !== JSON_INVALID_UTF8_SUBSTITUTE) {
            fastjson_validate($json, 512, $f);
            $got['validate'] = fastjson_last_error();
        }
        try {
            fastjson_decode($json, true, 512, $f | JSON_THROW_ON_ERROR);
            $got['throw'] = 0;
        } catch (JsonException $e) {
            $got['throw'] = $e->getCode();
        }
        fastjson_pointer_get($json, '/0');
        $got['pointer'] = fastjson_last_error();
        fastjson_merge_patch('{}', $json);
        $got['merge_patch'] = fastjson_last_error();
        foreach ($got as $api => $code) {
            if ($code !== $want) {
                $fail++;
                echo "FAIL ", bin2hex($json), " flags=$f $api: $code, want $want\n";
            }
        }
    }
    if ($want === JSON_ERROR_SYNTAX) {
        json_decode($json);
        if (json_last_error() !== JSON_ERROR_SYNTAX) {
            echo "ext/json disagrees on ", bin2hex($json), "\n";
        }
    }
}
$file = tempnam(sys_get_temp_dir(), 'fjesc');
foreach ($cases as [$json, $want]) {
    file_put_contents($file, $json);
    fastjson_file_decode($file);
    if (fastjson_last_error() !== $want) {
        $fail++;
        echo "FAIL ", bin2hex($json), " file_decode: ", fastjson_last_error(), ", want $want\n";
    }
}
unlink($file);
echo $fail === 0 ? "OK\n" : "FAILED $fail\n";
?>
--EXPECT--
OK
