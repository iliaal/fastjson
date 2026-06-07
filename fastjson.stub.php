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
 * Decode flag (fastjson-only, no ext/json equivalent): tolerate the
 * JSONC subset -- line/block comments, trailing commas, leading BOM.
 * @var int
 * @cvalue FASTJSON_DECODE_RELAXED
 */
const FASTJSON_DECODE_RELAXED = UNKNOWN;

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
 * Encodes $value to JSON and writes it to $filename in one call.
 *
 * Convenience wrapper for file_put_contents($filename,
 * fastjson_encode($value, ...)). The file is written through the PHP
 * streams layer, so stream wrappers and open_basedir are honored. The
 * write is NOT atomic. $flags and $depth carry the exact semantics of
 * fastjson_encode().
 *
 * Returns true on success, false on failure (encode error or I/O
 * error). On failure fastjson_last_error() is set; an I/O failure is
 * silent (no warning). JSON_THROW_ON_ERROR throws on an encode error;
 * an I/O failure still returns false.
 */
function fastjson_file_encode(string $filename, mixed $value, int $flags = 0, int $depth = 512): bool {}

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
 *   FASTJSON_DECODE_RELAXED       - tolerate the JSONC subset
 *                                   (line/block comments, trailing
 *                                   commas, leading BOM) that ext/json
 *                                   rejects; fastjson-only, no JSON_*
 *                                   equivalent
 *
 * The $depth parameter caps recursion. $depth <= 0 or > INT_MAX
 * raises a ValueError; the cap is enforced during the parse.
 */
function fastjson_decode(string $json, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed {}

/**
 * Reads $filename and decodes its contents as JSON in one call.
 *
 * Convenience wrapper for fastjson_decode(file_get_contents($filename),
 * ...). The file is read through the PHP streams layer, so stream
 * wrappers (php://, user-registered wrappers) and open_basedir are
 * honored. $associative, $depth, and $flags carry the exact semantics
 * of fastjson_decode().
 *
 * Returns the decoded value on success, or null on failure -- the same
 * contract as fastjson_decode(): callers check fastjson_last_error() to
 * distinguish a decoded-null from a failure. Failure covers both a
 * file that cannot be read (silent: no warning, fastjson_last_error()
 * set to a non-zero code with a descriptive message) and a JSON parse
 * error (translated code as usual). JSON_THROW_ON_ERROR throws on a
 * parse error; an I/O failure still returns null (a filesystem error
 * is not a JSON error).
 */
function fastjson_file_decode(string $filename, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed {}

/**
 * Reads a single value from $json at the given RFC 6901 JSON Pointer.
 *
 * Resolves the pointer against the parsed document and decodes only the
 * referenced subtree into a PHP value -- the rest of the document is
 * never materialized into PHP. Standard pointer syntax: "/users/0/email";
 * the empty pointer "" selects the whole document.
 *
 * Returns the value at the pointer, or null when the pointer does not
 * resolve (a missing path or a malformed pointer is treated as "no
 * value", not an error -- the error state is left clear). A pointer that
 * resolves to a JSON null also returns null; like the rest of the decode
 * family, callers cannot distinguish the two from PHP alone.
 *
 * On a JSON parse error, returns null with fastjson_last_error() set (or
 * throws under JSON_THROW_ON_ERROR). $associative, $depth, and $flags --
 * including FASTJSON_DECODE_RELAXED -- carry the same semantics as
 * fastjson_decode() and apply to the decoded subtree.
 */
function fastjson_pointer_get(string $json, string $pointer, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed {}

/**
 * Applies an RFC 7386 JSON Merge Patch ($patch) to $target and returns
 * the merged document as a PHP value.
 *
 * Merge semantics (RFC 7386): objects merge recursively; a non-object
 * patch replaces the target wholesale; a null member deletes the
 * corresponding key from the target. Returns a PHP value rather than a
 * JSON string so the result flows through the same single encoder -- pass
 * it to fastjson_encode() for byte-consistent output.
 *
 * On a JSON parse error in either operand, returns null with
 * fastjson_last_error() set (or throws under JSON_THROW_ON_ERROR).
 * $associative, $depth, and $flags -- including FASTJSON_DECODE_RELAXED
 * for the operand parse -- carry the same semantics as fastjson_decode().
 */
function fastjson_merge_patch(string $target, string $patch, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed {}

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
