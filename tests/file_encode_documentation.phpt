--TEST--
fastjson_file_encode: documented partial-output return and exception contract
--FILE--
<?php
$stub = file_get_contents(__DIR__ . '/../fastjson.stub.php');
preg_match('~/\*\*((?:(?!/\*\*).)*)\*/\s*function fastjson_file_encode\(~s', $stub, $match);
$doc = preg_replace('/\s+/', ' ', preg_replace('/^\s*\* ?/m', '', $match[1]));
$returns = 'Returns true when the produced JSON is successfully written, including '
    . 'partial output. With JSON_PARTIAL_OUTPUT_ON_ERROR, true can be returned '
    . 'while fastjson_last_error() reports the encoding error. Returns false if '
    . 'encoding produces no JSON or file I/O fails; exceptions are described '
    . 'below. On a false return, fastjson_last_error() is set.';
$exceptions = 'JSON_THROW_ON_ERROR throws on an encode error unless '
    . 'JSON_PARTIAL_OUTPUT_ON_ERROR is also set. A reported I/O failure returns '
    . 'false rather than throwing JsonException. Exceptions from user callbacks '
    . 'or stream wrappers propagate.';
var_dump(str_contains($doc, $returns));
var_dump(str_contains($doc, $exceptions));
?>
--EXPECT--
bool(true)
bool(true)
