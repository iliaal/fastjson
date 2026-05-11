/*
  +----------------------------------------------------------------------+
  | Copyright (c) 2026, Ilia Alshanetsky                                 |
  | Copyright (c) 2026, Advanced Internet Designs Inc.                   |
  +----------------------------------------------------------------------+
  | This source file is subject to the BSD 3-Clause license that is      |
  | bundled with this package in the file LICENSE.                       |
  +----------------------------------------------------------------------+
  | Author: Ilia Alshanetsky <ilia@ilia.ws>                              |
  +----------------------------------------------------------------------+
*/

#ifndef PHP_FASTJSON_H
#define PHP_FASTJSON_H

#define PHP_FASTJSON_VERSION "0.1.0"

extern zend_module_entry fastjson_module_entry;
#define phpext_fastjson_ptr &fastjson_module_entry

#ifdef PHP_WIN32
#define PHP_FASTJSON_API __declspec(dllexport)
#else
#define PHP_FASTJSON_API
#endif

#include "php.h"

/* ext/json compatibility: JSON_ERROR_* values are stable ints. fastjson
 * sets the same numeric codes so callers can use either ext/json's
 * constants or bare ints to compare against fastjson_last_error().
 *
 * Defined here as macros (not constants) to avoid registration
 * collisions when both extensions are loaded -- ext/json owns the
 * constant names; fastjson reuses the values internally. */
#define FASTJSON_ERROR_NONE              0
#define FASTJSON_ERROR_DEPTH             1
#define FASTJSON_ERROR_SYNTAX            4
#define FASTJSON_ERROR_UTF8              5
#define FASTJSON_ERROR_RECURSION         6
#define FASTJSON_ERROR_INF_OR_NAN        7
#define FASTJSON_ERROR_UNSUPPORTED_TYPE  8

/* Encode-side flag bits. Values match ext/json's JSON_* constants so
 * a caller passing JSON_PRETTY_PRINT works whether or not ext/json is
 * loaded; we reuse the bit positions on purpose. */
#define FASTJSON_ENCODE_HEX_TAG               (1 << 0)
#define FASTJSON_ENCODE_HEX_AMP               (1 << 1)
#define FASTJSON_ENCODE_HEX_APOS              (1 << 2)
#define FASTJSON_ENCODE_HEX_QUOT              (1 << 3)
#define FASTJSON_ENCODE_FORCE_OBJECT          (1 << 4)
#define FASTJSON_ENCODE_NUMERIC_CHECK         (1 << 5)
#define FASTJSON_ENCODE_UNESCAPED_SLASHES     (1 << 6)
#define FASTJSON_ENCODE_PRETTY_PRINT          (1 << 7)
#define FASTJSON_ENCODE_UNESCAPED_UNICODE     (1 << 8)
#define FASTJSON_ENCODE_PARTIAL_OUTPUT_ON_ERROR (1 << 9)
#define FASTJSON_ENCODE_PRESERVE_ZERO_FRACTION (1 << 10)
#define FASTJSON_ENCODE_INVALID_UTF8_IGNORE   (1 << 20)
#define FASTJSON_ENCODE_INVALID_UTF8_SUBSTITUTE (1 << 21)
#define FASTJSON_ENCODE_THROW_ON_ERROR        (1 << 22)

/* Decode-side flag bits (subset of ext/json's decode flags). */
#define FASTJSON_DECODE_OBJECT_AS_ARRAY       (1 << 0)
#define FASTJSON_DECODE_BIGINT_AS_STRING      (1 << 1)
#define FASTJSON_DECODE_INVALID_UTF8_IGNORE   (1 << 20)
#define FASTJSON_DECODE_INVALID_UTF8_SUBSTITUTE (1 << 21)
#define FASTJSON_DECODE_THROW_ON_ERROR        (1 << 22)

/* Cached class entries resolved at MINIT. Both come from ext/json
 * (which is always loaded in standard PHP builds); we do not register
 * our own copies. NULL on builds where ext/json is somehow absent --
 * fastjson degrades gracefully (throws \Exception, ignores
 * JsonSerializable). */
extern zend_class_entry *fastjson_json_exception_ce;
extern zend_class_entry *fastjson_json_serializable_ce;

/* Direct-write encoder entry point (fastjson_directwrite.c).
 * Returns the encoded zend_string on success (caller owns it), or
 * NULL on unrecoverable error (FASTJSON_G(last_err_code) set). */
zend_string *fastjson_directwrite_encode(zval *value, zend_long flags,
                                         zend_long depth);

ZEND_BEGIN_MODULE_GLOBALS(fastjson)
    zend_long last_err_code;
    /* yyjson's err.msg is a string literal owned by yyjson.c; we store
     * the pointer directly without copying. NULL means "no error
     * recorded yet this request" -- last_error_msg() returns "No error"
     * for both NULL and the success state. */
    const char *last_err_msg;
ZEND_END_MODULE_GLOBALS(fastjson)

ZEND_EXTERN_MODULE_GLOBALS(fastjson)

#define FASTJSON_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(fastjson, v)

#include "yyjson.h"

/* Translate a yyjson read code into the matching ext/json JSON_ERROR_*
 * int. SUCCESS -> NONE; INVALID_STRING -> UTF8 (yyjson's signal for bad
 * UTF-8 inside a JSON string); everything else -> SYNTAX. */
zend_long fastjson_translate_read_code(yyjson_read_code yy);

/* Record the most recent fastjson_* call's failure in module globals.
 * msg may be NULL; pointers are stored verbatim because yyjson's
 * err.msg points into static literals. */
void fastjson_set_error(yyjson_read_code code, const char *msg);

/* Reset module globals to JSON_ERROR_NONE / NULL. Called on every
 * successful fastjson_* call and on each RINIT. */
void fastjson_clear_error(void);

/* Encode-side: set a FASTJSON_ERROR_* code directly (no read-code
 * translation). msg may be NULL or a string literal -- not freed. */
void fastjson_set_encode_error(zend_long code, const char *msg);

/* Returns true if `s` contains an unquoted Inf, Infinity, or NaN
 * literal token (case-insensitive). Used after the overflow-retry
 * parse to reject inputs that mix overflow with a literal that
 * ext/json rejects. */
bool fastjson_input_has_inf_nan_literal(const char *s, size_t len);

/* Returns true if the bytes starting at s[pos] form a valid UTF-8
 * sequence: ASCII, or a complete and well-formed 2/3/4-byte
 * sequence with no overlong, surrogate, or out-of-range encodings.
 * Used to gate the parse-error UTF-8 reclassification -- yyjson
 * reports any unexpected character as a syntax issue, but ext/json
 * distinguishes valid-UTF-8-but-not-JSON (SYNTAX) from
 * malformed-UTF-8 (UTF8). */
bool fastjson_byte_is_valid_utf8_start(const char *s, size_t len, size_t pos);

#endif /* PHP_FASTJSON_H */
