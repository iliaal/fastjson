--TEST--
fastjson_encode: a hard-stop error inside the discard walk ends the traversal
--EXTENSIONS--
fastjson
--FILE--
<?php

/* A non-partial INF/NAN frees the buffer and keeps traversing so a later error
 * can take precedence, as ext/json does. ext/json aborts that walk as soon as a
 * nested value returns FAILURE (invalid UTF-8, recursion, unsupported type), so
 * containers outside the failing one are never visited. The discard walk has to
 * stop at the same point: otherwise the reported error code comes from a value
 * ext/json never reached, and JsonSerializable callbacks fire spuriously. */

class Probe implements JsonSerializable
{
    public static int $calls = 0;

    public function jsonSerialize(): mixed
    {
        self::$calls++;
        return 1;
    }
}

$bad = "\xB1\x31";
$resource = fopen('php://memory', 'r');
$recursive = [];
$recursive[] = &$recursive;

$cases = [
    [[[INF, $bad], INF]],
    [[[INF, $bad], NAN]],
    [['a' => [INF, $bad], 'b' => INF]],
    [[[INF, $bad], $resource]],
    [[[INF, $bad], $recursive]],
    [[[[INF, $bad]], INF]],
    [(object)['a' => [INF, $bad], 'b' => INF]],
    [[[INF, $bad], [[[1]]]]],
];

foreach ($cases as [$value]) {
    json_encode($value);
    $native = json_last_error();
    var_dump(fastjson_encode($value));
    var_dump(fastjson_last_error() === $native);
}

/* Callback counts must match ext/json exactly. */
$callbackCases = [
    static fn() => [[INF, $bad], new Probe()],
    static fn() => ['a' => [INF, $bad], 'b' => new Probe()],
    static fn() => [[[INF, $bad]], new Probe()],
    static fn() => [[INF], new Probe()],
    static fn() => [new Probe(), new Probe()],
];

foreach ($callbackCases as $make) {
    Probe::$calls = 0;
    json_encode($make());
    $nativeCalls = Probe::$calls;
    Probe::$calls = 0;
    fastjson_encode($make());
    var_dump(Probe::$calls === $nativeCalls);
}

/* PARTIAL_OUTPUT keeps the buffered walk and is unaffected. */
var_dump(fastjson_encode([[INF, $bad], INF], JSON_PARTIAL_OUTPUT_ON_ERROR)
    === json_encode([[INF, $bad], INF], JSON_PARTIAL_OUTPUT_ON_ERROR));

fclose($resource);
unset($recursive);
?>
--EXPECT--
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(false)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
