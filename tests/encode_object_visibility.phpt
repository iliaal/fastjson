--TEST--
fastjson_encode: only public properties (protected/private filtered), engine-object views match ext/json
--EXTENSIONS--
fastjson
--FILE--
<?php

class Plain {
    public int $pub = 1;
    protected int $pro = 2;
    private int $priv = 3;
}

// Protected/private must not appear. No NUL-mangled keys allowed.
$out = fastjson_encode(new Plain);
var_dump($out);
var_dump(json_encode(new Plain) === $out);

// Inherited visibility: private of base class stays hidden even on child instance.
class Child extends Plain {
    public int $extra = 10;
}
$out = fastjson_encode(new Child);
var_dump($out);
var_dump(json_encode(new Child) === $out);

// Engine objects: zend_get_properties_for(JSON) returns the right view.
$d = new DateTimeImmutable("2020-01-02T03:04:05+00:00");
var_dump(fastjson_encode($d) === json_encode($d));

$ao = new ArrayObject(["x" => 1, "y" => [2, 3]]);
var_dump(fastjson_encode($ao) === json_encode($ao));

// stdClass with cast-array origin: all keys are public, all must appear.
$o = (object)["a" => 1, "b" => 2];
var_dump(fastjson_encode($o));
?>
--EXPECT--
string(9) "{"pub":1}"
bool(true)
string(20) "{"pub":1,"extra":10}"
bool(true)
bool(true)
bool(true)
string(13) "{"a":1,"b":2}"
