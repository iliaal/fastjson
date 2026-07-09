--TEST--
fastjson_encode: jsonSerialize() unsetting the encoded array (bug77843 UAF)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Regression for a heap-use-after-free (ASAN: heap-use-after-free at
 * dw_emit_jsonserializable -> zend_call_method_with_0_params, matching
 * upstream bug77843). While encoding an object through a `&`-reference
 * array, the object's jsonSerialize() unset()s the array element holding
 * it -- dropping the last non-encoder reference. Without the encoder's
 * own reference the object is freed mid-call and the VM's ZEND_FETCH_THIS
 * reads freed memory. The fix holds a reference across the call.
 *
 * This is a memory-safety regression test: it asserts the encode
 * completes and the process survives (the assertion is meaningful under
 * the ASAN CI job). fastjson's exact output on a `&`-reference array
 * whose contents mutate mid-encode is intentionally not compared against
 * ext/json -- that interleaving divergence is tracked separately in
 * tests/upstream-json/.skiplist (bug77843). */

class ReturnScalar implements JsonSerializable {
    public $prop = "value";
    public function jsonSerialize(): mixed {
        global $arr;
        unset($arr[0]);      // drop the array's reference to $this
        return $this->prop;  // still dereferences $this
    }
}

$arr = [new ReturnScalar()];
$out = fastjson_encode([&$arr]);
var_dump(is_string($out));

class ReturnSelf implements JsonSerializable {
    public $prop = "value";
    public function jsonSerialize(): mixed {
        global $arr2;
        unset($arr2[0]);
        return $this;        // self-return: encodes own properties, no crash
    }
}

$arr2 = [new ReturnSelf()];
$out2 = fastjson_encode([&$arr2]);
var_dump(is_string($out2));

/* Hammer the path so a use-after-free reliably trips ASAN / the debug
 * allocator rather than surviving by luck on a single call. */
for ($i = 0; $i < 500; $i++) {
    $a = [new ReturnScalar()];
    $GLOBALS['arr'] = $a;
    fastjson_encode([&$a], JSON_PARTIAL_OUTPUT_ON_ERROR);
}
gc_collect_cycles();
echo "ok\n";
?>
--EXPECT--
bool(true)
bool(true)
ok
