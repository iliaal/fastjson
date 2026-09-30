--TEST--
chunked encode does not split a UTF-8 sequence on a chunk boundary
--EXTENSIONS--
fastjson
--FILE--
<?php

/* The leading quote forces the chunked writer (escaped, >= 8 KiB). Each
 * character is placed so it ends 0-3 bytes past a 2 KiB chunk cut. */
$chars = ["\u{e9}", "\u{4e00}", "\u{1f600}", "\xc3", "\xe4\xb8", "\xf0\x9f\x98"];
$flagSets = [
    0,
    JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES,
    JSON_INVALID_UTF8_SUBSTITUTE,
    JSON_INVALID_UTF8_IGNORE | JSON_UNESCAPED_UNICODE,
    JSON_PARTIAL_OUTPUT_ON_ERROR,
];
$checked = 0;
$bad = [];
foreach ([2048, 4096, 8192, 16384] as $cut) {
    foreach ($chars as $ci => $ch) {
        for ($back = 0; $back < 4; $back++) {
            $value = '"' . str_repeat('a', $cut - 1 - $back) . $ch
                . str_repeat("b\n", 4600);
            foreach ($flagSets as $flags) {
                $fast = fastjson_encode($value, $flags);
                $ext = json_encode($value, $flags);
                if ($fast !== $ext
                        || fastjson_last_error() !== json_last_error()) {
                    $bad[] = "$cut/$ci/$back/$flags";
                }
                $checked++;
            }
        }
    }
}
var_dump($checked, $bad);
?>
--EXPECT--
int(480)
array(0) {
}
