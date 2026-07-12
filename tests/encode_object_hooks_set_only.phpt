--TEST--
fastjson_encode: SET-only hook on non-virtual property (borrowed-zval refcount)
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID < 80400) die("skip property hooks require PHP 8.4"); ?>
--FILE--
<?php

/* Non-virtual property with a SET hook and no GET hook: the engine's
 * zend_std_read_property returns the backing field via OBJ_PROP() as a
 * borrowed pointer (no refcount bump). Treating that pointer as an owned
 * temporary would decrement the backing field's refcount and free the
 * underlying zend_string while the object still owns it -- UAF on the
 * next read.
 *
 * The `set => $value;` shorthand stores the parameter into the backing
 * field unchanged, so the encoded value equals the assigned value and
 * comparisons stay easy to read. */
class SetOnlyBacked {
    public string $hooked = "init" {
        set => $value;
    }
}

$o = new SetOnlyBacked;
$o->hooked = str_repeat('a', 64);          /* non-interned, refcount = 1 */

$out = fastjson_encode($o);
var_dump($out === json_encode($o));

/* The crucial check: the backing field's string must still be intact
 * after encoding. With the bug, refcount has been decremented to 0 and
 * the next read returns freed memory (or aborts under ASAN). */
var_dump($o->hooked === str_repeat('a', 64));

/* Encode many times to exercise the path repeatedly. With the bug the
 * second iteration would either re-free or assert. */
for ($i = 0; $i < 50; $i++) {
    fastjson_encode($o);
}
var_dump($o->hooked === str_repeat('a', 64));

/* Mixed case: non-virtual SET-only alongside a regular hooked property
 * and a plain field. All three must serialize, and the SET-only field's
 * backing storage must survive intact. */
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

/* Cycle the object through GC explicitly; if the backing field's
 * zend_string was freed, gc_collect_cycles would crash. */
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
