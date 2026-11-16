--TEST--
Non-ASCII strings: escape, copy, decode and validate at every offset around 16-byte blocks
--EXTENSIONS--
fastjson
--FILE--
<?php
$fills = [
    'cjk' => "\u{4E2D}\u{6587}\u{5B57}\u{7B26}\u{6D4B}\u{8BD5}",
    'cyr' => "\u{43F}\u{440}\u{438}\u{432}\u{435}\u{442} ",
    'mix' => "a\u{E9}\u{20AC}\u{1F600}b\u{FFFF}\u{E000}\u{D7FF}\u{800}\u{7FF}",
];
$stops = ['"', '\\', "\n", "\x01", '/', "\u{10FFFF}", 'x'];
$bad = ["\xC0\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xE2\x82", "\x80", "\xFF"];
$encFlags = [0, JSON_UNESCAPED_UNICODE, JSON_UNESCAPED_SLASHES, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES];
$subFlags = [JSON_INVALID_UTF8_SUBSTITUTE, JSON_INVALID_UTF8_IGNORE,
    JSON_INVALID_UTF8_SUBSTITUTE | JSON_UNESCAPED_UNICODE, JSON_INVALID_UTF8_IGNORE | JSON_UNESCAPED_UNICODE];
$fail = 0;
$check = function (string $what, $got, $want) use (&$fail) {
    if ($got !== $want && $fail++ < 5) {
        echo "FAIL $what\n";
    }
};
foreach ($fills as $name => $fill) {
    $body = str_repeat($fill, 8);
    for ($off = 0; $off <= 70; $off++) {
        $end = $off;
        while ($end > 0 && (ord($body[$end]) & 0xC0) === 0x80) {
            $end--;
        }
        $cut = substr($body, 0, $end);
        foreach ($stops as $stop) {
            $s = $cut . $stop . $body;
            foreach ($encFlags as $f) {
                $want = json_encode($s, $f);
                $check("$name/$off enc $f", fastjson_encode($s, $f), $want);
                $check("$name/$off dec $f", fastjson_decode($want), $s);
                $check("$name/$off key $f", fastjson_encode([$s => 1], $f), json_encode([$s => 1], $f));
            }
            $raw = '"' . addcslashes($cut, "\"\\\0..\37") . '\\n' . $body . '"';
            $check("$name/$off raw", fastjson_decode($raw), json_decode($raw));
            $check("$name/$off validate", fastjson_validate($raw), true);
        }
        foreach ($bad as $b) {
            $s = $cut . $b . $body;
            $check("$name/$off bad enc", fastjson_encode($s), false);
            $check("$name/$off bad err", fastjson_last_error(), JSON_ERROR_UTF8);
            foreach ($subFlags as $f) {
                $check("$name/$off sub $f", fastjson_encode($s, $f), json_encode($s, $f));
            }
            $j = '"' . $s . '"';
            $check("$name/$off bad dec", fastjson_decode($j), null);
            $check("$name/$off bad dec err", fastjson_last_error(), JSON_ERROR_UTF8);
            $check("$name/$off bad validate", fastjson_validate($j), false);
            $check("$name/$off bad dec sub", fastjson_decode($j, false, 512, JSON_INVALID_UTF8_SUBSTITUTE),
                json_decode($j, false, 512, JSON_INVALID_UTF8_SUBSTITUTE));
        }
    }
}
echo $fail === 0 ? "OK\n" : "FAILED $fail\n";
?>
--EXPECT--
OK
