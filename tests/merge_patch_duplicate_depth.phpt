--TEST--
fastjson_merge_patch checks depth after selecting last duplicate members
--EXTENSIONS--
fastjson
--FILE--
<?php
function checkMerge(string $target, string $patch, array $expected): void {
    foreach ([1, 2, 3] as $depth) {
        $result = fastjson_merge_patch($target, $patch, true, $depth);
        if ($depth === 1) {
            if ($result !== null || fastjson_last_error() !== JSON_ERROR_DEPTH) {
                throw new RuntimeException('root depth limit was ignored');
            }
        } elseif ($result !== $expected || fastjson_last_error() !== JSON_ERROR_NONE) {
            throw new RuntimeException('discarded duplicate affected result depth');
        }
    }
    fastjson_decode('{');
    $saved = fastjson_last_error_info();
    if (fastjson_merge_patch($target, $patch, true, 2, JSON_THROW_ON_ERROR) !== $expected
        || fastjson_last_error_info() !== $saved) {
        throw new RuntimeException('throw-mode result or saved error changed');
    }
}

// Exercise both sides of the 16-member linear/indexed dispatch boundary.
foreach ([2, 15, 16, 17] as $size) {
    $padding = '';
    $expected = ['a' => 1];
    for ($i = 0; $i < $size - 2; $i++) {
        $padding .= ',"k' . $i . '":0';
        $expected['k' . $i] = 0;
    }
    // The first member is deep, but only the final member survives.
    $duplicates = '{"a":[[0]]' . $padding . ',"a":1}';
    checkMerge($duplicates, '{}', $expected);
    checkMerge('{}', $duplicates, $expected);
    checkMerge('{"a":0}', $duplicates, $expected);

    // A final null deletes the member; escaped keys compare by decoded bytes.
    $deletions = '{"a":[[0]]' . $padding . ',"\u0061":null}';
    unset($expected['a']);
    checkMerge('{"a":0}', $deletions, $expected);

    // Last-wins must still reject a genuinely deep surviving member.
    $surviving = '{"a":1' . $padding . ',"a":[[0]]}';
    foreach ([[$surviving, '{}'], ['{}', $surviving]] as [$target, $patch]) {
        foreach ([2, 3, 4] as $depth) {
            $result = fastjson_merge_patch($target, $patch, true, $depth);
            if ($depth < 4) {
                if ($result !== null || fastjson_last_error() !== JSON_ERROR_DEPTH) {
                    throw new RuntimeException('surviving duplicate escaped depth check');
                }
            } elseif ($result !== ['a' => [[0]]] + $expected
                || fastjson_last_error() !== JSON_ERROR_NONE) {
                throw new RuntimeException('surviving duplicate rejected above depth limit');
            }
        }
    }
    echo "members=$size ok\n";
}

// Repeat the preflight inside a nested merge, not just at the root.
var_dump(fastjson_merge_patch(
    '{"outer":{"a":[[0]],"a":1}}', '{"outer":{}}', true, 3
) === ['outer' => ['a' => 1]]);
var_dump(fastjson_merge_patch(
    '{"outer":{}}', '{"outer":{"a":[[0]],"a":1}}', true, 3
) === ['outer' => ['a' => 1]]);
?>
--EXPECT--
members=2 ok
members=15 ok
members=16 ok
members=17 ok
bool(true)
bool(true)
