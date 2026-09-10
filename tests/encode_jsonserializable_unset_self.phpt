--TEST--
fastjson_encode: jsonSerialize() unsetting the encoded array (bug77843 UAF)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Dropping the last non-encoder reference must not free $this mid-call.
 * Assert survival under ASAN; output parity is excluded because mutation
 * interleaving differs from ext/json (upstream-json/.skiplist, bug77843). */

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

/* Repeat to expose use-after-free under ASAN or the debug allocator. */
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
