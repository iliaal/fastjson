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
 * fastjson_last_error() set).
 *
 * Supported $flags (others are accepted but currently ignored, with
 * the documented intent of matching ext/json's defaults):
 *   JSON_PRETTY_PRINT       - emit indented output (4-space indent)
 *   JSON_UNESCAPED_SLASHES  - don't escape forward slashes
 *   JSON_UNESCAPED_UNICODE  - emit non-ASCII as raw UTF-8 instead of
 *                             \uXXXX escapes
 *   JSON_FORCE_OBJECT       - encode any associative or list array
 *                             as a JSON object
 *   JSON_THROW_ON_ERROR     - throw JsonException on encode failure
 *                             instead of returning false
 *
 * The $depth parameter caps recursion. On overflow the function
 * returns false with fastjson_last_error() == FASTJSON_ERROR_DEPTH.
 * Cyclic references are also caught and reported as DEPTH.
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
 *   - false: JSON objects decode to stdClass (default).
 *   - null:  treated as false in v0.2; will respect a future
 *            JSON_OBJECT_AS_ARRAY flag once flags are wired.
 *
 * The $depth parameter is accepted for source-compatibility with
 * ext/json's json_decode() but is not currently enforced (see
 * todos/001). The $flags parameter is reserved.
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
 * ext/json's json_validate() but is not currently enforced; yyjson
 * has its own internal recursion guard. The $flags parameter is
 * reserved for future use.
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
