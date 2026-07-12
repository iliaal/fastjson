--TEST--
fastjson_encode: JsonSerializable returning an object with many hooked properties
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID < 80400) die("skip property hooks require PHP 8.4"); ?>
--FILE--
<?php

/* Regression coverage for callback-result ownership while hooked property
 * reads create temporary zvals. The callback object and every hook result
 * must stay alive for exactly the recursive encode that consumes it. */

function make_hooked(int $count): string {
    $props = '';
    for ($i = 0; $i < $count; $i++) {
        $props .= "    public int \$p{$i} { get => {$i}; }\n";
    }
    return $props;
}

eval("class Hooked {\n" . make_hooked(20) . "}\n");

class Wrap implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new Hooked();
    }
}

$w = new Wrap();

/* Output must match ext/json byte-for-byte. */
var_dump(fastjson_encode($w) === json_encode($w));

/* Encode repeatedly to exercise callback and hook cleanup under ASAN. */
for ($i = 0; $i < 100; $i++) {
    $r = fastjson_encode($w);
}
var_dump($r === json_encode($w));

/* Nested callback and hooked-object lifetimes. */
class WrapNested implements JsonSerializable {
    public function jsonSerialize(): mixed {
        $o = new Hooked();
        return ['outer' => $o, 'inner' => new Wrap()];
    }
}
$wn = new WrapNested();
var_dump(fastjson_encode($wn) === json_encode($wn));

/* Cycle through GC; a dangling/over-freed object would crash here. */
gc_collect_cycles();
echo "ok\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
ok
