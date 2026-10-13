--TEST--
fastjson_decode: a number with 768+ integer digits and a fraction rounds to the nearest double
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_INT_SIZE !== 8) die('skip needs 64-bit integers to compare IEEE-754 bits'); ?>
--FILE--
<?php
/* The midpoint between 1e30 and the next double up is
 * 1000000000000000090253369016320. Each input below scales one side of that
 * midpoint by 10^n with n trailing digits, then divides it back with e-n,
 * so the exact value sits just below or just above the midpoint. Past 768
 * significant digits the reader truncates the digit string; it must still
 * see the fraction that follows the cut. */
const MID_BELOW = '1000000000000000090253369016319';
const MID_ABOVE = '1000000000000000090253369016320';
const LOWER_BITS = 0x46293e5939a08cea;

function bits(float $d): int
{
    return unpack('J', pack('E', $d))[1];
}

$failures = 0;
$checked = 0;
foreach ([768, 769, 770, 771, 800] as $int_len) {
    $zeros = $int_len - strlen(MID_BELOW);
    foreach ([0, 1] as $side) {
        foreach ([false, true] as $negative) {
            $num = ($negative ? '-' : '')
                . ($side ? MID_ABOVE : MID_BELOW)
                . str_repeat($side ? '0' : '9', $zeros)
                . '.' . ($side ? '1' : '9')
                . ($side ? 'E' : 'e') . '-' . $zeros;
            $expected = (LOWER_BITS + $side) | ($negative ? PHP_INT_MIN : 0);
            foreach (['scalar' => $num, 'array' => "[$num]"] as $shape => $json) {
                $value = fastjson_decode($json, true);
                if ($shape === 'array') {
                    $value = is_array($value) ? $value[0] : $value;
                }
                $checked++;
                if (!is_float($value) || bits($value) !== $expected) {
                    $failures++;
                    printf("digits=%d side=%d negative=%d %s: got %s expected %016x\n",
                        $int_len, $side, $negative, $shape,
                        is_float($value) ? sprintf('%016x', bits($value)) : get_debug_type($value),
                        $expected);
                }
            }
        }
    }
}

/* An all-integer mantissa with the same digit count was already rounded
 * correctly; keep it pinned alongside the fractional cases. */
$zeros = 800 - strlen(MID_BELOW);
$value = fastjson_decode(MID_BELOW . str_repeat('9', $zeros) . 'e-' . $zeros);
$checked++;
if (!is_float($value) || bits($value) !== LOWER_BITS) {
    $failures++;
    echo "integer mantissa: wrong rounding\n";
}

var_dump(fastjson_last_error() === FASTJSON_ERROR_NONE);
echo "checked $checked, failures $failures\n";
?>
--EXPECT--
bool(true)
checked 41, failures 0
