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
 * @cvalue FASTJSON_ERROR_STATE_MISMATCH
 */
const FASTJSON_ERROR_STATE_MISMATCH = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_CTRL_CHAR
 */
const FASTJSON_ERROR_CTRL_CHAR = UNKNOWN;

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
 * @var int
 * @cvalue FASTJSON_ERROR_INVALID_PROPERTY_NAME
 */
const FASTJSON_ERROR_INVALID_PROPERTY_NAME = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_UTF16
 */
const FASTJSON_ERROR_UTF16 = UNKNOWN;

/**
 * @var int
 * @cvalue FASTJSON_ERROR_NON_BACKED_ENUM
 */
const FASTJSON_ERROR_NON_BACKED_ENUM = UNKNOWN;

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
 * fastjson_last_error() set). The false-on-failure behavior matches
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
 *   JSON_INVALID_UTF8_IGNORE      - drop malformed UTF-8 in strings
 *   JSON_INVALID_UTF8_SUBSTITUTE  - replace malformed UTF-8 with U+FFFD
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
 * error). On failure fastjson_last_error() is set. Fastjson does not ask
 * the streams layer to report open errors, but open_basedir, individual
 * wrappers, and write callbacks may emit their own warnings, as they do
 * under file_put_contents().
 * I/O failures use FASTJSON_ERROR_SYNTAX because
 * fastjson intentionally stays inside the JSON_ERROR_* code range; use
 * fastjson_last_error_msg() ("Failed to open file for writing" or
 * "Failed to write file") to distinguish them from encode failures.
 * JSON_THROW_ON_ERROR throws on an encode error; an I/O failure still
 * returns false.
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
 *   JSON_INVALID_UTF8_IGNORE      - drop malformed UTF-8 in strings
 *   JSON_INVALID_UTF8_SUBSTITUTE  - replace malformed UTF-8 with U+FFFD
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
 * file that cannot be read and a JSON parse error. Fastjson does not ask
 * the streams layer to report open errors, but open_basedir, individual
 * wrappers, and read callbacks may emit their own warnings, as they do
 * under file_get_contents(). I/O failure sets a non-zero code and a
 * descriptive message; a JSON parse
 * error (translated code as usual). I/O failures use
 * FASTJSON_ERROR_SYNTAX because fastjson intentionally stays inside
 * the JSON_ERROR_* code range; use fastjson_last_error_msg()
 * ("Failed to open file for reading" or "Failed to read file") to
 * distinguish them from parse errors. JSON_THROW_ON_ERROR throws on a
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
 * If an object traversed by the pointer contains the selected member more
 * than once, RFC 6901 evaluation is ambiguous. The function returns null with
 * FASTJSON_ERROR_SYNTAX, or throws under JSON_THROW_ON_ERROR. JSON parse
 * errors use the same error path. $associative, $depth, and $flags -- including
 * FASTJSON_DECODE_RELAXED -- otherwise carry the same semantics as
 * fastjson_decode() and apply to the decoded subtree.
 */
function fastjson_pointer_get(string $json, string $pointer, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed {}

/**
 * Reports whether the RFC 6901 JSON Pointer $pointer resolves to a value
 * in $json.
 *
 * Returns true when the pointer resolves (including when it resolves to a
 * JSON null -- unlike fastjson_pointer_get, the bool result disambiguates
 * "present but null" from "absent"), false when the path is missing or the
 * pointer is malformed. Nothing is materialized into PHP.
 *
 * A false return with fastjson_last_error() == FASTJSON_ERROR_NONE means
 * the path is absent. A duplicate selected member is ambiguous and sets
 * FASTJSON_ERROR_SYNTAX; JSON parse errors also set an error. Either error
 * throws under JSON_THROW_ON_ERROR. $flags carries the parse-affecting bits of
 * fastjson_decode() (e.g. FASTJSON_DECODE_RELAXED, JSON_THROW_ON_ERROR).
 */
function fastjson_pointer_exists(string $json, string $pointer, int $flags = 0): bool {}

/**
 * Sets the value at the RFC 6901 JSON Pointer $pointer in $json to $value
 * and returns the re-serialized JSON document.
 *
 * The parsed document is re-emitted by an immutable splice writer -- only
 * $value is materialized from PHP, not the whole input -- so this avoids a full
 * decode/re-encode round-trip for a single edit on a large document. Missing
 * parent objects are created; the empty pointer "" replaces the whole
 * document. Returns false (or throws under JSON_THROW_ON_ERROR) when $json
 * fails to parse, when $value cannot be encoded, or when the pointer cannot
 * resolve to a settable location (e.g. an array index gap or a scalar
 * mid-path). If an object on the pointer path contains the target member more
 * than once, the location is ambiguous and the operation fails instead of
 * replacing every duplicate.
 *
 * $depth is argument #4 and bounds nesting in the re-serialized result.
 * The replacement value consumes the budget remaining at its pointer path;
 * an input subtree that is replaced wholesale does not consume output depth.
 * JSON_PRETTY_PRINT, JSON_UNESCAPED_SLASHES,
 * JSON_UNESCAPED_UNICODE, and JSON_HEX_* format the whole output.
 * Value-transforming flags (JSON_NUMERIC_CHECK,
 * JSON_PRESERVE_ZERO_FRACTION, JSON_FORCE_OBJECT,
 * JSON_PARTIAL_OUTPUT_ON_ERROR, and JSON_INVALID_UTF8_IGNORE/SUBSTITUTE)
 * apply to $value only; untouched parsed values retain their JSON types and
 * numeric representation. FASTJSON_DECODE_RELAXED and JSON_THROW_ON_ERROR
 * apply to $json.
 *
 * Untouched number lexemes are preserved exactly. Untouched strings are
 * re-emitted by yyjson's writer, so documented escaping divergences such as
 * U+2028/U+2029 still apply to the whole output, not only the edited node.
 */
function fastjson_pointer_set(string $json, string $pointer, mixed $value, int $depth = 512, int $flags = 0): string|false {}

/**
 * Applies an RFC 7386 JSON Merge Patch ($patch) to $target and returns
 * the merged document as a PHP value.
 *
 * Merge semantics (RFC 7386): objects merge recursively; a non-object
 * patch replaces the target wholesale; a null member deletes the
 * corresponding key from the target. Returns a PHP value rather than a
 * JSON string so the result flows through the same single encoder -- pass
 * it to fastjson_encode() for byte-consistent output.
 * RFC 7386 does not define duplicate member handling. Fastjson canonicalizes
 * objects it merges using the last member's value and the first member's
 * insertion position, matching PHP's decoded-object model.
 *
 * On a JSON parse error in either operand, returns null with
 * fastjson_last_error() set (or throws under JSON_THROW_ON_ERROR).
 * $depth bounds the effective merged result: operand subtrees deleted or
 * replaced by the patch need not fit, while every retained subtree must fit.
 * $associative and $flags -- including FASTJSON_DECODE_RELAXED for the operand
 * parse -- otherwise carry the same semantics as fastjson_decode().
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
 * succeeded. Values and FASTJSON_ERROR_* aliases match ext/json's constants.
 * yyjson does not distinguish every ext/json parse category: raw control
 * characters and invalid UTF-16 surrogate escapes are currently reported as
 * FASTJSON_ERROR_UTF8 rather than CTRL_CHAR or UTF16.
 */
function fastjson_last_error(): int {}

/**
 * Returns the human-readable error message for the most recent
 * fastjson_* call in this request, or "No error" if no error is
 * recorded. The message is yyjson's diagnostic and is more
 * descriptive than ext/json's static error strings.
 */
function fastjson_last_error_msg(): string {}

/**
 * Returns the byte offset into the input at which the most recent
 * fastjson_* parse error occurred, or -1 when the last call succeeded or
 * the error has no source position (encode, file I/O, or depth errors).
 *
 * The offset indexes the raw input bytes; pair it with
 * fastjson_last_error_info() for the 1-based line and column.
 */
function fastjson_last_error_pos(): int {}

/**
 * Returns the most recent fastjson_* error as a structured array, folding
 * fastjson_last_error(), fastjson_last_error_msg(), and
 * fastjson_last_error_pos() into one call:
 *
 *   [
 *     'code' => int,    // FASTJSON_ERROR_* / JSON_ERROR_* code
 *     'msg'  => string, // "No error" when none
 *     'pos'  => int,    // byte offset, -1 when none/not-applicable
 *     'line' => int,    // 1-based line, 0 when unknown
 *     'col'  => int,    // 1-based column, 0 when unknown
 *   ]
 *
 * Line and column are populated for parse errors that carry a source
 * position; encode, file I/O, and depth errors report pos -1 / line 0 /
 * col 0.
 */
function fastjson_last_error_info(): array {}
