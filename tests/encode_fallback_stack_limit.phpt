--TEST--
fastjson_encode bounds scalar JsonSerializable chains
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID >= 80300) die('skip fallback applies before PHP 8.3'); ?>
--FILE--
<?php
final class ChainNode implements JsonSerializable {
    public ?self $next = null;

    public function jsonSerialize(): mixed {
        return $this->next ?? 1;
    }
}

$root = new ChainNode();
$node = $root;
for ($i = 0; $i < 1500; $i++) {
    $node->next = new ChainNode();
    $node = $node->next;
}

var_dump(fastjson_encode($root, 0, 100000));
var_dump(fastjson_last_error());
?>
--EXPECT--
bool(false)
int(1)
