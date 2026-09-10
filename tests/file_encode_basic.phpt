--TEST--
fastjson_file_encode: writes encoded JSON to a file; flags carry through; round-trip
--EXTENSIONS--
fastjson
--FILE--
<?php

$file = sys_get_temp_dir() . '/fastjson_file_encode_basic.json';

$data = ['name' => 'a/b', 'list' => [1, 2, 3]];

var_dump(fastjson_file_encode($file, $data));
echo file_get_contents($file), "\n";
var_dump(fastjson_last_error());

echo "---\n";

var_dump(fastjson_file_encode($file, $data, JSON_UNESCAPED_SLASHES));
echo file_get_contents($file), "\n";

echo "---\n";

var_dump(fastjson_file_encode($file, ['x' => 1], JSON_PRETTY_PRINT));
echo file_get_contents($file), "\n";

echo "---\n";

fastjson_file_encode($file, $data, JSON_UNESCAPED_SLASHES);
var_dump(fastjson_file_decode($file, true) === $data);

unlink($file);
?>
--EXPECT--
bool(true)
{"name":"a\/b","list":[1,2,3]}
int(0)
---
bool(true)
{"name":"a/b","list":[1,2,3]}
---
bool(true)
{
    "x": 1
}
---
bool(true)
