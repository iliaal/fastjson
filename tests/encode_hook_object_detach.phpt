--TEST--
Encoding holds a hooked object while a getter detaches it
--EXTENSIONS--
fastjson
--SKIPIF--
<?php
if (PHP_VERSION_ID < 80400) die('skip property hooks require PHP 8.4');
?>
--FILE--
<?php
final class HookDetachTarget {
    public string $first {
        get {
            $GLOBALS['holder']->node = null;
            return 'hook';
        }
    }
    public string $last = 'tail';
}
final class HookDetachHolder {
    public mixed $node;
}

foreach (['native', 'encode', 'file', 'pointer'] as $mode) {
    $holder = new HookDetachHolder();
    $holder->node = new HookDetachTarget();
    $weak = WeakReference::create($holder->node);
    $path = __DIR__ . '/encode_hook_object_detach.tmp';
    switch ($mode) {
        case 'native': $result = json_encode($holder); break;
        case 'encode': $result = fastjson_encode($holder); break;
        case 'file':
            var_dump(fastjson_file_encode($path, $holder));
            $result = file_get_contents($path);
            unlink($path);
            break;
        case 'pointer': $result = fastjson_pointer_set('{}', '', $holder); break;
    }
    echo "$mode: $result\n";
    var_dump($weak->get() === null);
}
?>
--CLEAN--
<?php
@unlink(__DIR__ . '/encode_hook_object_detach.tmp');
?>
--EXPECT--
native: {"node":{"first":"hook","last":"tail"}}
bool(true)
encode: {"node":{"first":"hook","last":"tail"}}
bool(true)
bool(true)
file: {"node":{"first":"hook","last":"tail"}}
bool(true)
pointer: {"node":{"first":"hook","last":"tail"}}
bool(true)
