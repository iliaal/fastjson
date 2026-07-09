--TEST--
fastjson_encode: jsonSerialize() growing the encoded array (arData realloc UAF)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Regression for a heap-use-after-free while encoding an array that a
 * nested JsonSerializable mutates through an aliasing `&`-reference. When
 * the aliased array has refcount 1, appending to it inside jsonSerialize()
 * reallocates its packed arData in place, dangling the ZEND_HASH_FOREACH
 * cursor dw_emit_array holds. Valgrind (USE_ZEND_ALLOC=0) flagged the
 * invalid read at dw_emit_array before the fix; the encoder now holds a
 * reference across the descent so the mutation copies-on-write away.
 *
 * The unset()-based sibling (dropping the element) is covered separately
 * in encode_jsonserializable_unset_self.phpt; this exercises the growth
 * path that reallocates storage. Assertion: the process survives and the
 * iterated snapshot encodes -- meaningful under the ASAN CI job. */

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

/* Hammer it so a use-after-free reliably trips ASAN / the debug allocator
 * rather than surviving by luck on a single call. */
for ($i = 0; $i < 300; $i++) {
    $g = [1, new Grow(), 2];
    fastjson_encode([&$g]);
}
echo "ok\n";
?>
--EXPECT--
bool(true)
ok
