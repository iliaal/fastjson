--TEST--
fastjson_encode() with JSON_PRETTY_PRINT
--EXTENSIONS--
fastjson
--FILE--
<?php
function encode_decode($json) {
    $struct = fastjson_decode($json);
    $pretty = fastjson_encode($struct, JSON_PRETTY_PRINT);
    echo "$pretty\n";
    $pretty = fastjson_decode($pretty);
    printf("Match: %d\n", $pretty == $struct);
}

encode_decode('[1,2,3,[1,2,3]]');
encode_decode('{"a":1,"b":[1,2],"c":{"d":42}}');
?>
--EXPECT--
[
    1,
    2,
    3,
    [
        1,
        2,
        3
    ]
]
Match: 1
{
    "a": 1,
    "b": [
        1,
        2
    ],
    "c": {
        "d": 42
    }
}
Match: 1
