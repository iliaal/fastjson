--TEST--
Benchmark JSON preflights compare types after default ext/json oracle normalization
--EXTENSIONS--
fastjson
--INI--
serialize_precision=-1
--FILE--
<?php
// Exercise the real validators without running their timed fixtures.
foreach ([
    ['review-performance.php', 'function reviewValidate(', '$packedScalar ='],
    ['perf_ideas.php', 'function validate_bench_cases(', '$dataset ='],
] as [$file, $startMarker, $endMarker]) {
    $source = file_get_contents(dirname(__DIR__) . '/bench/' . $file);
    $start = strpos($source, $startMarker);
    $end = strpos($source, $endMarker);
    if ($start === false || $end === false || $start >= $end) {
        throw new RuntimeException('missing benchmark validator');
    }
    eval(substr($source, $start, $end - $start));
}

// Literal result strings independently pin the accepted normalization, rather
// than deriving both sides with the same json_encode flags as the validator.
$fixtures = [
    [12345.0, ['12345'], ['12345.0', '12345e0', '"12345"', 'true']],
    [1.0, ['1'], ['1.0', '1e0', '"1"']],
    [-1.0, ['-1'], ['-1.0', '"-1"']],
    [0.0, ['0', '-0'], ['0.0', '-0.0', 'false', 'null']],
    [-0.0, ['-0', '0'], ['-0.0', '0.0']],
    [1.25, ['1.25', '1.250', '1.25e0'], ['1', '"1.25"']],
    [1e20, ['1e20', '1.0e20'], ['1', '"1e20"']],
    [1, ['1'], ['1.0', '"1"', 'true']],
    ['1', ['"1"', '"\u0031"'], ['1', '1.0']],
    [true, ['true'], ['1', '"true"']],
    [null, ['null'], ['false', '0', '"null"']],
    [[], ['[]'], ['{}']],
    [(object)[], ['{}'], ['[]']],
    [[1.0, [], (object)[]], ['[1,[],{}]', " [ 1, [], {} ]\n"],
        ['[1.0,[],{}]', '[1,{},{}]', '[1,[],[]]']],
    [['value' => 1.0], ['{"value":1}'], ['{"value":1.0}', '{"value":"1"}']],
    [(object)['items' => [1.0]], ['{"items":[1]}'],
        ['{"items":[1.0]}', '{"items":{"0":1}}']],
    // Object-key order remains significant, as in the existing validators.
    [['a' => 1, 'b' => 2], ['{"a":1,"b":2}'], ['{"b":2,"a":1}']],
];
$validators = [
    'review' => static fn(array $case) => reviewValidate('normalization', $case),
    'ideas' => static fn(array $case) => validate_bench_cases([$case]),
];
foreach ($validators as $name => $validate) {
    $accepted = $rejected = 0;
    foreach ($fixtures as [$expected, $valid, $invalid]) {
        $case = ['name' => 'normalization', 'expected' => $expected, 'json' => true,
            'fn' => static fn() => fastjson_encode($expected)];
        // Default native encoding must satisfy the normalized oracle too.
        $validate($case);
        foreach ($valid as $json) {
            $case['fn'] = static fn() => $json;
            $validate($case);
            $accepted++;
        }
        foreach ($invalid as $json) {
            $case['fn'] = static fn() => $json;
            try {
                $validate($case);
                throw new LogicException("accepted $name normalization mismatch: $json");
            } catch (RuntimeException $error) {
                if ($error->getMessage() !== 'cannot benchmark normalization: unexpected result') throw $error;
            }
            $rejected++;
        }
    }
    // Non-JSON results are not normalized: PHP float 1.0 remains a float.
    $case = ['name' => 'normalization', 'expected' => 1.0, 'fn' => static fn() => 1.0];
    $validate($case);
    $case['fn'] = static fn() => 1;
    try {
        $validate($case);
        throw new LogicException("accepted $name non-JSON type mismatch");
    } catch (RuntimeException $error) {
        if ($error->getMessage() !== 'cannot benchmark normalization: unexpected result') throw $error;
    }
    echo "$name: ", count($fixtures), " native fixtures, $accepted JSON variants, $rejected rejections; raw float stays strict\n";
}
?>
--EXPECT--
review: 17 native fixtures, 24 JSON variants, 39 rejections; raw float stays strict
ideas: 17 native fixtures, 24 JSON variants, 39 rejections; raw float stays strict
