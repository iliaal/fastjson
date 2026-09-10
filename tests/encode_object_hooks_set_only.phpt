--TEST--
fastjson_encode: SET-only hook on non-virtual property (borrowed-zval refcount)
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID < 80400) die("skip property hooks require PHP 8.4"); ?>
--FILE--
<?php

/* SET-only backed properties return borrowed zvals; releasing them as
 * temporaries would free storage still owned by the object. */
class SetOnlyBacked {
    public string $hooked = "init" {
        set => $value;
    }
}

$o = new SetOnlyBacked;
$o->hooked = str_repeat('a', 64);          /* non-interned, refcount = 1 */

$out = fastjson_encode($o);
var_dump($out === json_encode($o));

/* Reading after encode exposes premature release of the backing string. */
var_dump($o->hooked === str_repeat('a', 64));

/* Repeat to expose double-free or refcount assertions. */
for ($i = 0; $i < 50; $i++) {
    fastjson_encode($o);
}
var_dump($o->hooked === str_repeat('a', 64));

class MixedBacked {
    public int $n = 1;
    public string $stored = "raw" {
        set => $value;
    }
    public string $derived { get => "hi"; }
}
$m = new MixedBacked;
$m->stored = str_repeat('x', 32);
$out = fastjson_encode($m);
var_dump($out === json_encode($m));
var_dump($m->stored === str_repeat('x', 32));

/* GC must also tolerate the backing storage after encode. */
gc_collect_cycles();
var_dump($o->hooked === str_repeat('a', 64));
var_dump($m->stored === str_repeat('x', 32));

echo "OK\n";
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
OK
