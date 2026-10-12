--TEST--
Encoding holds a plain object while a nested serializer detaches it
--EXTENSIONS--
fastjson
--FILE--
<?php
final class DetachChild implements JsonSerializable {
    public function jsonSerialize(): mixed {
        $GLOBALS['holder']->node = null;
        if ($GLOBALS['weak']->get() === null) {
            throw new RuntimeException('object released during serialization');
        }
        return 'child';
    }
}
final class DetachTarget {
    public mixed $first;
    public string $last = 'tail';
    public function __construct() { $this->first = new DetachChild(); }
}
final class DetachHolder {
    public mixed $node;
}

foreach (['native', 'encode', 'file', 'pointer'] as $mode) {
    $holder = new DetachHolder();
    $holder->node = new DetachTarget();
    $weak = WeakReference::create($holder->node);
    $path = __DIR__ . '/encode_object_detach.tmp';
    try {
        switch ($mode) {
            case 'native': $result = json_encode($holder); break;
            case 'encode': $result = fastjson_encode($holder); break;
            case 'file':
                if (!fastjson_file_encode($path, $holder)) {
                    throw new RuntimeException('file encode failed');
                }
                $result = file_get_contents($path);
                unlink($path);
                break;
            case 'pointer': $result = fastjson_pointer_set('{}', '', $holder); break;
        }
        echo "$mode: $result\n";
        var_dump($weak->get() === null);
    } catch (Throwable $e) {
        echo "$mode: ", $e->getMessage(), "\n";
    }
}
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/encode_object_detach.tmp');
?>
--EXPECT--
native: {"node":{"first":"child","last":"tail"}}
bool(true)
encode: {"node":{"first":"child","last":"tail"}}
bool(true)
file: {"node":{"first":"child","last":"tail"}}
bool(true)
pointer: {"node":{"first":"child","last":"tail"}}
bool(true)
