--TEST--
Review benchmark preflight checks all fixture values, errors and exceptions
--EXTENSIONS--
fastjson
--FILE--
<?php
namespace ReviewPerformanceValidation;
use \RuntimeException;
use \Throwable;
use \stdClass;

function fastjson_last_error(): int
{
    return $GLOBALS['forcedError'] ?? \fastjson_last_error();
}
$source = file_get_contents(dirname(__DIR__) . '/bench/review-performance.php');
$end = strpos($source, '$selectedCases = [];');
if ($end === false) throw new RuntimeException('missing case definitions');
$argv = [__FILE__];
eval('namespace ' . __NAMESPACE__ . '; use \\RuntimeException; use \\Throwable; use \\stdClass;'
    . substr($source, 5, $end - 5));

$count = 0;
$rejected = 0;
foreach ($cases as $name => $case) {
    // Real native callbacks must agree with the independently prepared oracles.
    reviewValidate($name, $case);
    $count++;
    $GLOBALS['forcedError'] = $case['error'] ?? JSON_ERROR_NONE;
    $wrong = [false, null, 0, 1, true, [], new stdClass(), '', 'null', '{}', '[]', '"wrong"'];
    if ($case['json'] ?? false) $wrong[] = $case['expected'];
    if (is_array($case['expected'])) {
        $changed = $case['expected'];
        $first = array_key_first($changed);
        $changed[$first] = 'wrong';
        $wrong[] = ($case['json'] ?? false) ? json_encode($changed) : $changed;
    } elseif ($case['expected'] instanceof stdClass) {
        $changed = clone $case['expected'];
        $changed->{'key-0'} = '0';
        $wrong[] = $changed;
        $wrong[] = (array)$case['expected'];
    }
    if (($case['json'] ?? false) && is_array($case['expected'])) {
        if (array_is_list($case['expected'])) {
            $wrong[] = json_encode((object)$case['expected']);
        } elseif (isset($case['expected']['rows'])) {
            $changed = $case['expected'];
            $changed['rows'] = (object)$changed['rows'];
            $wrong[] = json_encode($changed);
        }
    }
    if ($name === 'encode_hex_32k' || $name === 'pointer_set_hex_32k') {
        $value = $name === 'encode_hex_32k' ? $hex32k : ['value' => $hex32k];
        $flags = JSON_HEX_TAG | JSON_HEX_AMP | JSON_HEX_APOS | JSON_HEX_QUOT;
        foreach ([0, $flags ^ JSON_HEX_TAG, $flags ^ JSON_HEX_AMP,
            $flags ^ JSON_HEX_APOS, $flags ^ JSON_HEX_QUOT] as $mask) {
            $wrong[] = json_encode($value, $mask);
        }
    }
    $normalizedExpected = ($case['json'] ?? false)
        ? serialize(json_decode(json_encode($case['expected']))) : null;
    foreach ($wrong as $value) {
        // These fixtures intentionally include successful true/null values.
        if (($case['json'] ?? false)
            ? is_string($value) && serialize(json_decode($value))
                === $normalizedExpected
                && json_last_error() === JSON_ERROR_NONE
            : $value === $case['expected']) continue;
        $bad = $case;
        $bad['fn'] = static fn() => $value;
        try {
            reviewValidate($name, $bad);
            throw new \LogicException("accepted wrong result for $name");
        } catch (RuntimeException $error) {
            if (!str_starts_with($error->getMessage(), "cannot benchmark $name: ")) throw $error;
        }
        $rejected++;
    }
    $GLOBALS['forcedError'] = ($case['error'] ?? JSON_ERROR_NONE) === JSON_ERROR_NONE
        ? JSON_ERROR_SYNTAX : JSON_ERROR_NONE;
    try {
        reviewValidate($name, $case);
        throw new \LogicException("accepted wrong error for $name");
    } catch (RuntimeException $error) {
        if ($error->getMessage() !== "cannot benchmark $name: unexpected error " . $GLOBALS['forcedError']) throw $error;
    }
    $bad = $case;
    $bad['fn'] = static function (): never { throw new \ValueError('callback failed'); };
    try {
        reviewValidate($name, $bad);
        throw new \LogicException('ignored callback exception');
    } catch (RuntimeException $error) {
        if ($error->getMessage() !== "cannot benchmark $name: callback failed"
            || !$error->getPrevious() instanceof \ValueError) throw $error;
    }
    unset($GLOBALS['forcedError']);
}
echo "$count native fixtures, $rejected wrong results, $count wrong errors and $count exceptions passed\n";
?>
--EXPECT--
41 native fixtures, 538 wrong results, 41 wrong errors and 41 exceptions passed
