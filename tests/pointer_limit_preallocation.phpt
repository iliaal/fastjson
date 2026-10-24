--TEST--
JSON Pointer limit rejection preserves syntax precedence and skips replacement callbacks
--EXTENSIONS--
fastjson
--FILE--
<?php
class LimitReplacement implements JsonSerializable {
    public static int $calls = 0;
    public function jsonSerialize(): mixed {
        self::$calls++;
        return 1;
    }
}
$replacement = new LimitReplacement();
$checked = 0;
foreach ([1, 2, 511, 512, 4095, 4096, 4097, 5000] as $count) {
    foreach (['', '~0', '~1', '~01', '~', '~2', '~~0', '~0~', "~\0", "\0", '~01~10'] as $tail) {
        $pointer = str_repeat('/', $count) . $tail;
        $valid = !preg_match('/~(?![01])/', $pointer);
        foreach ([2, 512] as $depth) {
            $result = fastjson_pointer_get('null', $pointer, true, $depth);
            $error = $valid && ($count > 4096 || $count >= $depth) ? JSON_ERROR_DEPTH : JSON_ERROR_NONE;
            if ($result !== null || fastjson_last_error() !== $error) die("get mismatch\n");
            $result = fastjson_pointer_set('null', $pointer, $replacement, $depth);
            $error = $valid && ($count > 4096 || $count >= $depth) ? JSON_ERROR_DEPTH : JSON_ERROR_SYNTAX;
            if ($result !== false || fastjson_last_error() !== $error) die("set mismatch\n");
            $checked += 2;
        }
        $result = fastjson_pointer_exists('null', $pointer);
        $error = $valid && $count > 4096 ? JSON_ERROR_DEPTH : JSON_ERROR_NONE;
        if ($result !== false || fastjson_last_error() !== $error) die("exists mismatch\n");
        $checked++;
    }
}
echo $checked, " contracts\n";
var_dump(LimitReplacement::$calls);
// Throw mode preserves the entry error state for both rejection kinds.
foreach (['', '~2', '~'] as $tail) {
    fastjson_decode('[');
    $saved = fastjson_last_error_info();
    try {
        fastjson_pointer_set('{}', str_repeat('/', 5000) . $tail, $replacement, 512, JSON_THROW_ON_ERROR);
    } catch (JsonException $e) {
        echo $e->getCode(), ':', fastjson_last_error_info() === $saved ? 'preserved' : 'changed', "\n";
    }
}
var_dump(LimitReplacement::$calls);
// Empty pointers and ordinary escaped names are unaffected.
var_dump(fastjson_pointer_get('7', '', true, 1));
var_dump(fastjson_pointer_get('{"a/b":{"~":3}}', '/a~1b/~0', true));
?>
--EXPECT--
440 contracts
int(0)
1:preserved
4:preserved
4:preserved
int(0)
int(7)
int(3)
