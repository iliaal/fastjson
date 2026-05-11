--TEST--
fastjson_encode: PHP 8.4 property hooks are invoked, output matches ext/json
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID < 80400) die("skip property hooks require PHP 8.4"); ?>
--FILE--
<?php

class Hooked {
    public string $x { get => "v"; }
    public int $y { get => 42; }
}

// Read-only hook: getter invoked, value serialized.
$out = fastjson_encode(new Hooked);
var_dump($out);
var_dump($out === json_encode(new Hooked));

// Hook combined with a plain public property: both appear.
class HookedMix {
    public string $plain = "p";
    public string $derived { get => strtoupper($this->plain); }
}
$m = new HookedMix;
$out = fastjson_encode($m);
var_dump($out);
var_dump($out === json_encode($m));

// Hook that throws: exception propagates, encode returns false-ish
// (here: throw propagates).
class HookedThrows {
    public string $x { get => throw new RuntimeException("nope"); }
}
try {
    fastjson_encode(new HookedThrows);
    echo "no throw\n";
} catch (RuntimeException $e) {
    echo "got: ", $e->getMessage(), "\n";
}

// Same hook under JSON_THROW_ON_ERROR: user exception still wins
// over a fastjson-side JsonException.
try {
    fastjson_encode(new HookedThrows, JSON_THROW_ON_ERROR);
    echo "no throw\n";
} catch (RuntimeException $e) {
    echo "got: ", $e->getMessage(), "\n";
}
?>
--EXPECT--
string(16) "{"x":"v","y":42}"
bool(true)
string(27) "{"plain":"p","derived":"P"}"
bool(true)
got: nope
got: nope
