<?php

/** @generate-class-entries */

/**
 * @var int
 * @cvalue FASTJSON_ERROR_NONE
 */
const FASTJSON_ERROR_NONE = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_DEPTH
 */
const FASTJSON_ERROR_DEPTH = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_SYNTAX
 */
const FASTJSON_ERROR_SYNTAX = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_UTF8
 */
const FASTJSON_ERROR_UTF8 = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_RECURSION
 */
const FASTJSON_ERROR_RECURSION = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_INF_OR_NAN
 */
const FASTJSON_ERROR_INF_OR_NAN = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_UNSUPPORTED_TYPE
 */
const FASTJSON_ERROR_UNSUPPORTED_TYPE = UNKNOWN;

/**
 * Returns the fastjson extension version string (matches
 * PHP_FASTJSON_VERSION in php_fastjson.h).
 */
function fastjson_version(): string {}

/**
 * Encodes a PHP value to a JSON string.
 *
 * Returns the JSON string on success, or false on failure (with
 * fastjson_last_error() set). Failure returns null behavior matches
 * ext/json's json_encode().
 *
 * Supported $flags (all honored at the byte-equality level with
 * ext/json on the common path):
 *   JSON_PRETTY_PRINT             - 4-space indented output
 *   JSON_UNESCAPED_SLASHES        - don't escape forward slashes
 *   JSON_UNESCAPED_UNICODE        - emit non-ASCII as raw UTF-8
 *                                   instead of \uXXXX escapes
 *   JSON_FORCE_OBJECT             - encode any array as a JSON object
 *   JSON_HEX_TAG / HEX_AMP /
 *   HEX_APOS / HEX_QUOT           - substitute targeted chars with
 *                                   \u00XX escapes
 *   JSON_NUMERIC_CHECK            - emit numeric-looking strings as
 *                                   JSON numbers
 *   JSON_PRESERVE_ZERO_FRACTION   - keep ".0" on integer-valued
 *                                   doubles (default strips it)
 *   JSON_PARTIAL_OUTPUT_ON_ERROR  - substitute on per-value errors
 *                                   instead of aborting
 *   JSON_THROW_ON_ERROR           - throw JsonException on encode
 *                                   failure (instead of returning
 *                                   false); global error state is
 *                                   preserved per ext/json's contract
 *
 * The $depth parameter caps recursion. On overflow the function
 * returns false with fastjson_last_error() == FASTJSON_ERROR_RECURSION
 * (for cyclic references) or FASTJSON_ERROR_DEPTH (for plain depth
 * overflow). $depth is validated lazily on the first container, so
 * fastjson_encode($x, 0, 0) returns false rather than raising.
 */
function fastjson_encode(mixed $value, int $flags = 0, int $depth = 512): string|false {}

/**
 * Decodes a JSON string into a PHP value.
 *
 * Returns the decoded value (mixed: null, bool, int, float, string,
 * array, or stdClass) on success, or null on parse failure (with
 * fastjson_last_error() set). Since null is also a valid decoded
 * value, callers must check fastjson_last_error() to distinguish a
 * decoded-null from a parse failure.
 *
 * $associative selects the object representation:
 *   - true:  JSON objects decode to associative arrays.
 *   - false: JSON objects decode to stdClass.
 *   - null (default): pivots on JSON_OBJECT_AS_ARRAY in $flags;
 *     without the bit, JSON objects decode to stdClass.
 *
 * Supported $flags:
 *   JSON_OBJECT_AS_ARRAY          - decode objects as assoc arrays
 *                                   (only honored when $associative
 *                                   is null)
 *   JSON_BIGINT_AS_STRING         - emit integers that overflow PHP
 *                                   int range as strings instead of
 *                                   widening to double
 *   JSON_THROW_ON_ERROR           - throw JsonException on parse
 *                                   failure; global error state is
 *                                   preserved per ext/json's contract
 *
 * The $depth parameter caps recursion. $depth <= 0 or > INT_MAX
 * raises a ValueError; the cap is enforced during the parse.
 */
function fastjson_decode(string $json, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed {}

/**
 * Validates that the given string is well-formed JSON.
 * Returns true on success, false on any parse error.
 *
 * On failure, fastjson_last_error() and fastjson_last_error_msg()
 * carry an ext/json-compatible JSON_ERROR_* code and yyjson's
 * error message. On success, the error state is reset.
 *
 * The $depth parameter is accepted for source-compatibility with
 * ext/json's json_validate() but is not enforced on the success
 * path: doing so would require walking the parsed doc and double
 * the success-path cost. The argument is still validated (positive
 * in-range) and raises ValueError on $depth <= 0 or > INT_MAX.
 *
 * The only $flags value accepted is JSON_INVALID_UTF8_IGNORE. Any
 * other bits raise a ValueError, matching ext/json's contract.
 */
function fastjson_validate(string $json, int $depth = 512, int $flags = 0): bool {}

/**
 * Returns the JSON_ERROR_* code for the most recent fastjson_*
 * call in this request, or JSON_ERROR_NONE (0) if the last call
 * succeeded. Values match ext/json's constants by design.
 */
function fastjson_last_error(): int {}

/**
 * Returns the human-readable error message for the most recent
 * fastjson_* call in this request, or "No error" if no error is
 * recorded. The message is yyjson's diagnostic and is more
 * descriptive than ext/json's static error strings.
 */
function fastjson_last_error_msg(): string {}
