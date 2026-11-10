--TEST--
Review-performance encode cases use the intended escaped bytes and payload sizes
--FILE--
<?php
namespace ReviewPerformanceInputs;

// Inspect the inputs supplied by the actual case callbacks, without requiring
// the native extension or measuring wall-clock performance.
function fastjson_encode($value, int $flags = 0) { return $value; }

$source = file_get_contents(dirname(__DIR__) . '/bench/review-performance.php');
$start = strpos($source, '$packedScalar =');
$end = strpos($source, '$selectedCases = [];');
if ($start === false || $end === false || $end <= $start) {
    throw new \RuntimeException('missing review benchmark fixture definitions');
}
eval('namespace ' . __NAMESPACE__ . ';' . substr($source, $start, $end - $start));

$pattern = chr(34) . chr(92) . chr(10); // quote, backslash, newline
foreach (['128k' => 128 * 1024, '256k' => 256 * 1024,
          '512k' => 512 * 1024, '1m' => 1024 * 1024] as $suffix => $size) {
    $input = $cases['encode_escaped_' . $suffix]['fn']();
    $expected = str_repeat($pattern, intdiv($size, 3));
    echo "escaped_$suffix: ", $input === $expected ? 'ok' : 'wrong bytes or size', "\n";
    // Sibling inputs must keep their established payloads.
    $input = $cases['encode_unicode_' . $suffix]['fn']();
    echo "unicode_$suffix: ", $input === str_repeat("é", intdiv($size, 2)) ? 'ok' : 'wrong bytes or size', "\n";
    $input = $cases['encode_ascii_' . $suffix]['fn']();
    echo "ascii_$suffix: ", $input === str_repeat('a', $size) ? 'ok' : 'wrong bytes or size', "\n";
}
foreach (['256k' => 256 * 1024, '512k' => 512 * 1024] as $suffix => $size) {
    $input = $cases['encode_late_quote_' . $suffix]['fn']();
    echo "late_quote_$suffix: ", $input === str_repeat('a', $size - 1) . '"' ? 'ok' : 'wrong bytes or size', "\n";
}
$input = $cases['encode_hex_32k']['fn']();
echo 'hex_32k: ', $input === str_repeat('<>&\'"', intdiv(32 * 1024, 5)) ? 'ok' : 'wrong bytes or size', "\n";
?>
--EXPECT--
escaped_128k: ok
unicode_128k: ok
ascii_128k: ok
escaped_256k: ok
unicode_256k: ok
ascii_256k: ok
escaped_512k: ok
unicode_512k: ok
ascii_512k: ok
escaped_1m: ok
unicode_1m: ok
ascii_1m: ok
late_quote_256k: ok
late_quote_512k: ok
hex_32k: ok
