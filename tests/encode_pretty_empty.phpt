--TEST--
fastjson_encode: JSON_PRETTY_PRINT keeps empty containers compact
--EXTENSIONS--
fastjson
--FILE--
<?php

/* Empty arrays / objects must render compact ([] and {}) with no inner
 * newline even under JSON_PRETTY_PRINT, and a non-empty container that
 * holds empty ones must indent only the non-empty levels -- byte-for-byte
 * with ext/json. */

/* A class whose only properties are private/protected serializes to no
 * JSON members, so pretty-print must render it compact ({}) -- the raw
 * property count is non-zero, so emptiness has to be judged by what is
 * actually emitted, not by zend_array_count. */
class PrivOnly { private $a = 1; protected $b = 2; }

$cases = [
    'empty array'        => [],
    'empty object'       => new stdClass,
    'private-only object'=> new PrivOnly,
    'nested empties'     => ["a" => [], "b" => [1], "c" => new stdClass],
    'object with empties'=> (object)["x" => [], "y" => (object)[]],
    'mixed priv+public'  => new class { private $h = 9; public $v = 1; },
    'list of empties'    => [[], [], [1, 2]],
];

foreach ($cases as $label => $value) {
    $fj = fastjson_encode($value, JSON_PRETTY_PRINT);
    $ej = json_encode($value, JSON_PRETTY_PRINT);
    printf("%-20s %s\n", $label, $fj === $ej ? "match" : "DIFF\n  fj=$fj\n  ej=$ej");
}
?>
--EXPECT--
empty array          match
empty object         match
private-only object  match
nested empties       match
object with empties  match
mixed priv+public    match
list of empties      match
