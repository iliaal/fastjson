--TEST--
fastjson pointer reads reject duplicate members after UTF-8 normalization
--EXTENSIONS--
fastjson
--FILE--
<?php

$ignore = JSON_INVALID_UTF8_IGNORE;
$substitute = JSON_INVALID_UTF8_SUBSTITUTE;
$cases = [
    ['ignore', "{\"admin\":false,\"adm\xFFin\":true}", '/admin', $ignore],
    ['reverse', "{\"adm\xFFin\":false,\"admin\":true}", '/admin', $ignore],
    ['both invalid', "{\"adm\xFFin\":false,\"ad\xC0\xAFmin\":true}", '/admin', $ignore],
    ['empty', "{\"\":false,\"\xFF\":true}", '/', $ignore],
    ['nested', "{\"account\":{\"role\":false},\"acc\xFFount\":{\"role\":true}}", '/account/role', $ignore],
    ['substitute', "{\"adm\u{FFFD}in\":false,\"adm\xFFin\":true}", "/adm\u{FFFD}in", $substitute],
    ['both flags', "{\"adm\u{FFFD}in\":false,\"adm\xFFin\":true}", "/adm\u{FFFD}in", $ignore | $substitute],
];

foreach ($cases as [$name, $json, $pointer, $flags]) {
    $value = fastjson_pointer_get($json, $pointer, true, 512, $flags);
    $getError = fastjson_last_error();
    $exists = fastjson_pointer_exists($json, $pointer, $flags);
    $existsError = fastjson_last_error();
    echo $name, ': ';
    var_dump($value === null && $getError === JSON_ERROR_SYNTAX
        && !$exists && $existsError === JSON_ERROR_SYNTAX);
}

$json = "{\"admin\":false,\"adm\xFFin\":true,\"safe\":7}";
var_dump(fastjson_decode($json, true, 512, $ignore)['admin']);
var_dump(json_decode($json, true, 512, $ignore)['admin']);
var_dump(fastjson_pointer_get($json, '/safe', true, 512, $ignore));
var_dump(fastjson_pointer_exists($json, '/safe', $ignore));
var_dump(fastjson_pointer_get($json, '/missing', true, 512, $ignore));
var_dump(fastjson_last_error() === JSON_ERROR_NONE);

foreach (['get', 'exists'] as $operation) {
    fastjson_decode('"' . "\xFF" . '"');
    $saved = fastjson_last_error_info();
    try {
        if ($operation === 'get') {
            fastjson_pointer_get($json, '/admin', true, 512, $ignore | JSON_THROW_ON_ERROR);
        } else {
            fastjson_pointer_exists($json, '/admin', $ignore | JSON_THROW_ON_ERROR);
        }
        echo "did not throw\n";
    } catch (JsonException $e) {
        echo $operation, ': ', $e->getCode(), ': ', $e->getMessage(), "\n";
        var_dump(fastjson_last_error_info() === $saved);
    }
}
?>
--EXPECT--
ignore: bool(true)
reverse: bool(true)
both invalid: bool(true)
empty: bool(true)
nested: bool(true)
substitute: bool(true)
both flags: bool(true)
bool(true)
bool(true)
int(7)
bool(true)
NULL
bool(true)
get: 4: JSON pointer target is ambiguous because the object contains duplicate members
bool(true)
exists: 4: JSON pointer target is ambiguous because the object contains duplicate members
bool(true)
