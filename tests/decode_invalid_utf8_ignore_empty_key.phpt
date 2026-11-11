--TEST--
fastjson_decode handles object keys that JSON_INVALID_UTF8_IGNORE empties
--EXTENSIONS--
fastjson
--FILE--
<?php

$cases = [
    "{\"\xff\xfe\":1}",
    "{\"\xff\":1,\"\xfe\":2}",
    "{\"\xff7\":1,\"8\xfe\":2}",
    "{\"\xff\xfe\":1,\"5\":2}",
];

foreach ($cases as $json) {
    foreach ([true, false] as $assoc) {
        $got = fastjson_decode($json, $assoc, 512, JSON_INVALID_UTF8_IGNORE);
        $want = json_decode($json, $assoc, 512, JSON_INVALID_UTF8_IGNORE);
        echo bin2hex($json), ' ', $assoc ? 'assoc' : 'object', ': ',
            $got == $want && serialize($got) === serialize($want) ? 'OK' : 'BAD', "\n";
    }
}

var_dump(fastjson_decode("{\"\xff\xfe\":1}", true, 512, JSON_INVALID_UTF8_IGNORE));
?>
--EXPECT--
7b22fffe223a317d assoc: OK
7b22fffe223a317d object: OK
7b22ff223a312c22fe223a327d assoc: OK
7b22ff223a312c22fe223a327d object: OK
7b22ff37223a312c2238fe223a327d assoc: OK
7b22ff37223a312c2238fe223a327d object: OK
7b22fffe223a312c2235223a327d assoc: OK
7b22fffe223a312c2235223a327d object: OK
array(1) {
  [""]=>
  int(1)
}
