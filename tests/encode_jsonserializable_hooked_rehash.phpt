--TEST--
fastjson_encode: JsonSerializable returning a hooked object that rehashes the retval stash
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID < 80400) die("skip property hooks require PHP 8.4"); ?>
--FILE--
<?php

/* Regression: dw_emit_jsonserializable stashes the jsonSerialize() result
 * in ctx->retval_stash and passes the interior stash pointer down to
 * dw_emit_object as `zv`. When that result is an OBJECT with GET-hooked
 * properties, each hook value is inserted into the SAME stash. Once the
 * inserts cross the stash's initial capacity (8), the HashTable rehashes
 * and relocates arData -- dangling `zv`. The pre-fix code then read
 * Z_OBJ_P(zv) from the freed slot on every later property: a
 * use-after-free. The object must therefore have enough hooked
 * properties to force at least one rehash mid-loop. */

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

/* Encode repeatedly: with the bug, later iterations read the relocated
 * (freed) stash slot. Under ASAN this aborts; in a plain build it is
 * latent corruption. The fix captures the zend_object before the loop. */
for ($i = 0; $i < 100; $i++) {
    $r = fastjson_encode($w);
}
var_dump($r === json_encode($w));

/* Nested: a JsonSerializable whose result is a hooked object that itself
 * contains another wrapped hooked object, so the stash holds several live
 * interior pointers across rehashes. */
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
