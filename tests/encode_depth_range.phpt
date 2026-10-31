--TEST--
fastjson_encode/fastjson_file_encode: $depth outside the int range raises ValueError
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_INT_SIZE != 8) die("skip 64-bit only: zend_long equals int on 32-bit"); ?>
--FILE--
<?php

function attempt(callable $fn): string {
    try {
        return 'returned ' . var_export($fn(), true);
    } catch (Throwable $e) {
        return $e::class . ': ' . $e->getMessage();
    }
}

/* Containers, scalars, and the throw flag all fail the same way: the range
 * check runs before encoding, so it does not depend on the value or flags. */
foreach ([2147483648, -2147483649, PHP_INT_MAX, PHP_INT_MIN] as $depth) {
    echo attempt(fn() => fastjson_encode([1], 0, $depth)), "\n";
    echo attempt(fn() => fastjson_encode(1, 0, $depth)), "\n";
    echo attempt(fn() => fastjson_encode([1], JSON_THROW_ON_ERROR, $depth)), "\n";
}

/* The int bounds themselves keep their existing meaning. */
var_dump(fastjson_encode([[1]], 0, 2147483647));
var_dump(fastjson_encode([1], 0, -2147483648), fastjson_last_error() === JSON_ERROR_DEPTH);

/* The ValueError leaves the previously recorded error untouched. */
fastjson_decode('{');
$before = fastjson_last_error_info();
echo attempt(fn() => fastjson_encode([1], 0, 2147483648)), "\n";
var_dump(fastjson_last_error_info() === $before);

/* fastjson_file_encode() rejects the depth before touching the file. */
$path = sys_get_temp_dir() . '/fastjson-depth-range-' . bin2hex(random_bytes(6)) . '.json';
echo attempt(fn() => fastjson_file_encode($path, [1], 0, 2147483648)), "\n";
var_dump(file_exists($path));
file_put_contents($path, 'keep');
echo attempt(fn() => fastjson_file_encode($path, [1], 0, -2147483649)), "\n";
var_dump(file_get_contents($path));
unlink($path);

/* Where ext/json carries the same check, the messages agree. */
try {
    json_encode([1], 0, 2147483648);
    echo "json_encode: no range check in this PHP\n";
} catch (ValueError $e) {
    try {
        fastjson_encode([1], 0, 2147483648);
    } catch (ValueError $f) {
        echo str_replace('json_encode()', 'fastjson_encode()', $e->getMessage()) === $f->getMessage()
            ? "json_encode: same message\n" : "json_encode: MESSAGE DIFFERS\n";
    }
}
?>
--EXPECTF--
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
string(5) "[[1]]"
bool(false)
bool(true)
ValueError: fastjson_encode(): Argument #3 ($depth) must be between -2147483648 and 2147483647
bool(true)
ValueError: fastjson_file_encode(): Argument #4 ($depth) must be between -2147483648 and 2147483647
bool(false)
ValueError: fastjson_file_encode(): Argument #4 ($depth) must be between -2147483648 and 2147483647
string(4) "keep"
json_encode: %r(same message|no range check in this PHP)%r
