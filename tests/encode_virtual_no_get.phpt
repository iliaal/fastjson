--TEST--
fastjson_encode: virtual properties without a get hook are skipped (matches ext/json)
--EXTENSIONS--
fastjson
--SKIPIF--
<?php if (PHP_VERSION_ID < 80400) die("skip property hooks require PHP 8.4"); ?>
--FILE--
<?php

// Set-only hook makes $virt a virtual property with no get hook.
// ext/json skips it; fastjson must do the same instead of calling
// the (nonexistent) reader and including undef in the output.
class SetOnly {
    public string $plain = "p";
    public string $virt {
        set(string $v) { $this->plain = strtolower($v); }
    }
}
$o = new SetOnly;
var_dump(fastjson_encode($o));
var_dump(fastjson_encode($o) === json_encode($o));

// Mix: virtual-no-get and a regular hooked property.
class MixedHooks {
    public int $n = 1;
    public string $virt { set(string $v) { /* noop */ } }
    public string $derived { get => "hi"; }
}
$m = new MixedHooks;
var_dump(fastjson_encode($m));
var_dump(fastjson_encode($m) === json_encode($m));
?>
--EXPECT--
string(13) "{"plain":"p"}"
bool(true)
string(22) "{"n":1,"derived":"hi"}"
bool(true)
