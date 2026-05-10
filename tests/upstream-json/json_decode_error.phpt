--TEST--
Test fastjson_decode() function : error conditions
--EXTENSIONS--
fastjson
--FILE--
<?php
echo "*** Testing fastjson_decode() : error conditions ***\n";

echo "\n-- Testing fastjson_decode() function with depth below 0 --\n";

try {
    var_dump(fastjson_decode('"abc"', true, -1));
} catch (\ValueError $e) {
    echo $e->getMessage() . \PHP_EOL;
}

?>
--EXPECT--
*** Testing fastjson_decode() : error conditions ***

-- Testing fastjson_decode() function with depth below 0 --
fastjson_decode(): Argument #3 ($depth) must be greater than 0
