--TEST--
fastjson_encode: JsonSerializable chain that rehashes the retval stash (UAF)
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Regression: dw_emit_jsonserializable stashes the jsonSerialize() result
 * in ctx->retval_stash and, when that result is itself a JsonSerializable
 * object, passes the interior stash pointer down as `zv`. Each nested
 * level inserts into the SAME stash; once the inserts cross the stash's
 * initial capacity (8), the HashTable rehashes and relocates arData --
 * dangling every `zv` that points at an earlier slot. The pre-fix code
 * then read Z_UNPROTECT_RECURSION_P(zv) from the freed slot while
 * unwinding each level: a use-after-free (ASAN: heap-use-after-free at
 * dw_emit_jsonserializable). The chain must be deep enough to force at
 * least one rehash while early frames are still live. */

class Node implements JsonSerializable {
    public function __construct(private ?Node $next, private int $v) {}
    public function jsonSerialize(): mixed {
        return $this->next ?? $this->v;
    }
}

$head = null;
for ($i = 0; $i < 30; $i++) {
    $head = new Node($head, $i);
}

var_dump(fastjson_encode($head) === json_encode($head));

/* Cycle through GC; a dangling/over-freed object would crash here. */
gc_collect_cycles();
echo "ok\n";
?>
--EXPECT--
bool(true)
ok
