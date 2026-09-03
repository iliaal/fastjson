--TEST--
fastjson_encode/file_encode restore the entry error state on throw-mode success after nested encodes
--EXTENSIONS--
fastjson
--FILE--
<?php
final class ThrowSuccessTemporary {
    public int $value = 1;

    public function __destruct() {
        fastjson_encode(INF);
    }
}

final class ThrowSuccessOuter implements JsonSerializable {
    public function jsonSerialize(): mixed {
        return new ThrowSuccessTemporary();
    }
}

fastjson_decode('{');
$saved = fastjson_last_error();
var_dump(fastjson_encode(new ThrowSuccessOuter(), JSON_THROW_ON_ERROR));
var_dump(fastjson_last_error() === $saved);

$path = tempnam(sys_get_temp_dir(), 'fjtserr');
fastjson_decode('{');
$savedFile = fastjson_last_error();
var_dump(fastjson_file_encode($path, new ThrowSuccessOuter(), JSON_THROW_ON_ERROR));
var_dump(file_get_contents($path));
var_dump(fastjson_last_error() === $savedFile);
unlink($path);
--CLEAN--
<?php
foreach (glob(sys_get_temp_dir() . '/fjtserr*') as $f) { unlink($f); }
?>
--EXPECT--
string(11) "{"value":1}"
bool(true)
bool(true)
string(11) "{"value":1}"
bool(true)
