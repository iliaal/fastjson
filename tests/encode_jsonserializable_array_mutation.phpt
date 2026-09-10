--TEST--
fastjson_encode: jsonSerialize() growing the encoded array (arData realloc UAF)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Growth through an alias must copy-on-write instead of reallocating the
 * array under the encoder's hash cursor. ASAN checks for use-after-free. */

class Grow implements JsonSerializable {
    public function jsonSerialize(): mixed {
        global $g;
        for ($i = 0; $i < 512; $i++) {
            $g[] = str_repeat('y', 24);
        }
        return 1;
    }
}

$g = [100, new Grow(), 300, 400];
$out = fastjson_encode([&$g]);
var_dump(is_string($out));

/* Repeat to expose use-after-free under ASAN or the debug allocator. */
for ($i = 0; $i < 300; $i++) {
    $g = [1, new Grow(), 2];
    fastjson_encode([&$g]);
}
echo "ok\n";
?>
--EXPECT--
bool(true)
ok
