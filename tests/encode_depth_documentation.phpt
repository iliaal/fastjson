--TEST--
fastjson_encode: documented depth and recursion flag contract
--EXTENSIONS--
fastjson
--FILE--
<?php
$stub = file_get_contents(__DIR__ . '/../fastjson.stub.php');
preg_match('~/\*\*((?:(?!/\*\*).)*)\*/\s*function fastjson_encode\(~s', $stub, $match);
$doc = preg_replace('/\s+/', ' ', preg_replace('/^\s*\* ?/m', '', $match[1]));
$claims = [
    'The $depth parameter limits container nesting. Without '
    . 'JSON_PARTIAL_OUTPUT_ON_ERROR or JSON_THROW_ON_ERROR, depth and recursion '
    . 'errors return false and set fastjson_last_error() to FASTJSON_ERROR_DEPTH '
    . 'or FASTJSON_ERROR_RECURSION, respectively. JSON_THROW_ON_ERROR instead '
    . 'throws JsonException while preserving the previous error state. '
    . 'JSON_PARTIAL_OUTPUT_ON_ERROR takes precedence: it substitutes null for an '
    . 'over-depth subtree or recursive reference and records the error. Unlike '
    . 'ext/json, fastjson truncates over-depth subtrees in partial output.',
    'The depth limit is checked when encoding a container, not a scalar. '
    . 'For example, fastjson_encode([], 0, 0) returns false without raising '
    . 'ValueError, while fastjson_encode(42, 0, 0) returns "42". JsonSerializable '
    . "results and backed enum values use their serialized value's container depth.",
];
foreach ($claims as $claim) {
    var_dump(str_contains($doc, $claim));
}
$cycle = [];
$cycle[0] = &$cycle;
$cases = [
    [[[1]], 1, '[null]', FASTJSON_ERROR_DEPTH],
    [$cycle, 512, '[null]', FASTJSON_ERROR_RECURSION],
];
foreach ($cases as [$value, $depth, $partial, $error]) {
    foreach ([0, JSON_THROW_ON_ERROR, JSON_PARTIAL_OUTPUT_ON_ERROR,
              JSON_THROW_ON_ERROR | JSON_PARTIAL_OUTPUT_ON_ERROR] as $flags) {
        // Seed a different error to verify throw-mode preserves all error fields.
        fastjson_decode('{');
        $before = fastjson_last_error_info();
        try {
            $result = fastjson_encode($value, $flags, $depth);
            $expected = ($flags & JSON_PARTIAL_OUTPUT_ON_ERROR) ? $partial : false;
            var_dump($flags !== JSON_THROW_ON_ERROR
                && $result === $expected && fastjson_last_error() === $error);
        } catch (JsonException $e) {
            var_dump($flags === JSON_THROW_ON_ERROR && $e->getCode() === $error
                && fastjson_last_error_info() === $before);
        }
    }
}
class ScalarJson implements JsonSerializable {
    public function jsonSerialize(): mixed { return 42; }
}
enum ScalarEnum: int { case Value = 42; }
foreach ([[], new stdClass(), 42, new ScalarJson(), ScalarEnum::Value] as $value) {
    $container = is_array($value) || $value instanceof stdClass;
    var_dump(fastjson_encode($value, 0, 0) === ($container ? false : '42'));
    var_dump(fastjson_last_error() === ($container ? FASTJSON_ERROR_DEPTH : FASTJSON_ERROR_NONE));
}
?>
--EXPECT--
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
bool(true)
