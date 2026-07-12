--TEST--
fastjson_encode: nested JsonSerializable callback results remain stable
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Regression coverage for nested callback-result ownership. Each result
 * must remain alive through its recursive encode and be released while its
 * owning frame is still active. */

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
