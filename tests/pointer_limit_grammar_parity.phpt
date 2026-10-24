--TEST--
JSON Pointer limit preflight and normal plan parsing agree on escape grammar
--EXTENSIONS--
fastjson
--FILE--
<?php
// Use the normal plan parser as the oracle, not a third copy of its grammar.
// On {}, every valid nonempty pointer creates an object path successfully.
// The same spelling must keep syntax-error precedence when limits reject it.
$alphabet = ['~', '0', '1', '/', 'x', "\0"];
$level = [''];
$checked = 0;
$overLimit = str_repeat('/', 4097);
for ($length = 0; $length <= 5; $length++) {
    foreach ($level as $suffix) {
        $pointer = '/' . $suffix;
        $result = fastjson_pointer_set('{}', $pointer, 1, 512);
        $error = fastjson_last_error();
        if (($result === false && $error !== JSON_ERROR_SYNTAX)
            || ($result !== false && $error !== JSON_ERROR_NONE)) {
            die("unexpected normal-plan result\n");
        }
        $valid = $result !== false;
        if (fastjson_pointer_set('{}', $pointer, 1, 1) !== false
            || fastjson_last_error() !== ($valid ? JSON_ERROR_DEPTH : JSON_ERROR_SYNTAX)) {
            die("depth preflight mismatch: " . bin2hex($pointer) . "\n");
        }
        $oversized = $overLimit . $suffix;
        if (fastjson_pointer_set('{}', $oversized, 1, 512) !== false
            || fastjson_last_error() !== ($valid ? JSON_ERROR_DEPTH : JSON_ERROR_SYNTAX)) {
            die("segment set preflight mismatch: " . bin2hex($pointer) . "\n");
        }
        if (fastjson_pointer_get('{}', $oversized, true, 512) !== null
            || fastjson_last_error() !== ($valid ? JSON_ERROR_DEPTH : JSON_ERROR_NONE)) {
            die("segment get preflight mismatch: " . bin2hex($pointer) . "\n");
        }
        if (fastjson_pointer_exists('{}', $oversized) !== false
            || fastjson_last_error() !== ($valid ? JSON_ERROR_DEPTH : JSON_ERROR_NONE)) {
            die("segment exists preflight mismatch: " . bin2hex($pointer) . "\n");
        }
        $checked++;
    }
    if ($length < 5) {
        $next = [];
        foreach ($level as $suffix) {
            foreach ($alphabet as $char) {
                $next[] = $suffix . $char;
            }
        }
        $level = $next;
    }
}
echo $checked, " grammar cases agree\n";
?>
--EXPECT--
9331 grammar cases agree
