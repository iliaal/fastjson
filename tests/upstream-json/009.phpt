--TEST--
fastjson_encode() with non-packed array that should be encoded as an array rather than object
--EXTENSIONS--
fastjson
--FILE--
<?php
$a = array(1, 2, 3, 'foo' => 'bar');
unset($a['foo']);

var_dump(fastjson_encode($a));
echo "Done\n";
?>
--EXPECT--
string(7) "[1,2,3]"
Done

