--TEST--
fastjson_encode: container-depth boundary matches json_encode
--EXTENSIONS--
fastjson
--FILE--
<?php

/* The encoder counts containers (not scalar leaves). This walks the
 * exact boundary against json_encode for several nested shapes at every
 * depth from 1 up to just past what each shape needs -- the encode
 * analogue of decode_depth.phpt. An off-by-one in the container-only
 * counting would surface as a mismatch here. */

$shapes = [
    'list-3'   => [[[1]]],                 // 3 containers
    'obj-3'    => ["a" => ["b" => ["c" => 1]]],
    'mixed-4'  => [["k" => [[1]]]],         // 4 containers
    'flat-1'   => [1, 2, 3],                // 1 container
];

foreach ($shapes as $label => $value) {
    for ($d = 1; $d <= 6; $d++) {
        $fj = fastjson_encode($value, 0, $d);
        $ej = json_encode($value, 0, $d);
        if ($fj !== $ej) {
            printf("DIFF %s d=%d fj=%s ej=%s\n",
                $label, $d, var_export($fj, true), var_export($ej, true));
        }
    }
}
echo "depth-parity ok\n";

/* PARTIAL_OUTPUT_ON_ERROR: a self-referential array substitutes `null`
 * for the recursive edge, byte-for-byte with ext/json. */
$cyclic = [];
$cyclic[0] = &$cyclic;
$fj = fastjson_encode($cyclic, JSON_PARTIAL_OUTPUT_ON_ERROR);
$ej = json_encode($cyclic, JSON_PARTIAL_OUTPUT_ON_ERROR);
var_dump($fj === $ej, $fj);

/* PARTIAL_OUTPUT_ON_ERROR + depth exceeded: an INTENTIONAL divergence
 * from ext/json. ext/json ignores the depth cap for output under partial
 * output (it emits the full structure and only sets the depth error);
 * fastjson truncates the over-deep subtree with `null` and sets the same
 * error. This test pins fastjson's behavior so the divergence stays a
 * deliberate choice, not an accident. Both set JSON_ERROR_DEPTH. */
$deep = [[[[1]]]];
$fj = fastjson_encode($deep, JSON_PARTIAL_OUTPUT_ON_ERROR, 2);
var_dump($fj, fastjson_last_error() === JSON_ERROR_DEPTH);

/* $depth above INT_MAX: ext/json wraps its int max_depth non-positive, so
 * every container trips JSON_ERROR_DEPTH. fastjson matches (containers
 * fail), while a huge depth on a bare scalar still encodes cleanly. */
$huge = 2147483648; // INT_MAX + 1
var_dump(fastjson_encode([1], 0, $huge) === json_encode([1], 0, $huge));
var_dump(fastjson_last_error() === JSON_ERROR_DEPTH);
var_dump(fastjson_encode(1, 0, $huge) === json_encode(1, 0, $huge));
?>
--EXPECT--
depth-parity ok
bool(true)
string(6) "[null]"
string(8) "[[null]]"
bool(true)
bool(true)
bool(true)
bool(true)
