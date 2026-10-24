--TEST--
Oversized JSON Pointers are rejected without allocating the segment plan
--EXTENSIONS--
fastjson
--FILE--
<?php
// Input allocation is outside the measured call. The former parser allocated
// 16 MB of segments plus a 1 MB storage buffer before enforcing the cap.
$pointer = str_repeat('/', 1000000);
foreach (['get', 'exists', 'set'] as $operation) {
    if (function_exists('memory_reset_peak_usage')) memory_reset_peak_usage();
    $before = memory_get_usage();
    $result = match ($operation) {
        'get' => fastjson_pointer_get('{}', $pointer),
        'exists' => fastjson_pointer_exists('{}', $pointer),
        'set' => fastjson_pointer_set('{}', $pointer, 1),
    };
    $extra = memory_get_peak_usage() - $before;
    echo $operation, ':', fastjson_last_error() === JSON_ERROR_DEPTH ? 'depth' : 'wrong error', ':',
        $extra < 4 * 1024 * 1024 ? 'bounded' : 'oversized allocation', "\n";
}
?>
--EXPECT--
get:depth:bounded
exists:depth:bounded
set:depth:bounded
