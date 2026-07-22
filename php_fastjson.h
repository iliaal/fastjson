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

#define PHP_FASTJSON_VERSION "0.6.0"

extern zend_module_entry fastjson_module_entry;
#define phpext_fastjson_ptr &fastjson_module_entry

#include "php.h"
#include "Zend/zend_smart_str.h"

/* ext/json compatibility: JSON_ERROR_* values are stable ints. fastjson
 * sets the same numeric codes so callers can use either ext/json's
 * constants or bare ints to compare against fastjson_last_error().
 *
 * Defined here as macros (not constants) to avoid registration
 * collisions when both extensions are loaded -- ext/json owns the
 * constant names; fastjson reuses the values internally. */
#define FASTJSON_ERROR_NONE              0
#define FASTJSON_ERROR_DEPTH             1
#define FASTJSON_ERROR_STATE_MISMATCH    2
#define FASTJSON_ERROR_CTRL_CHAR         3
#define FASTJSON_ERROR_SYNTAX            4
#define FASTJSON_ERROR_UTF8              5
#define FASTJSON_ERROR_RECURSION         6
#define FASTJSON_ERROR_INF_OR_NAN        7
#define FASTJSON_ERROR_UNSUPPORTED_TYPE  8
#define FASTJSON_ERROR_INVALID_PROPERTY_NAME 9
#define FASTJSON_ERROR_UTF16             10
#define FASTJSON_ERROR_NON_BACKED_ENUM   11

/* JSON_INVALID_UTF8_IGNORE / _SUBSTITUTE occupy the same two bit
 * positions on the encode and decode sides (they mirror ext/json's
 * shared JSON_* values). The low-level sanitizer and the hot-path check
 * below key off these side-neutral names; the ENCODE_/DECODE_ aliases
 * further down exist for call-site readability and are defined in terms
 * of these so the bit positions live in exactly one place. */
#define FASTJSON_INVALID_UTF8_IGNORE          (1 << 20)
#define FASTJSON_INVALID_UTF8_SUBSTITUTE      (1 << 21)

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
#define FASTJSON_ENCODE_INVALID_UTF8_IGNORE   FASTJSON_INVALID_UTF8_IGNORE
#define FASTJSON_ENCODE_INVALID_UTF8_SUBSTITUTE FASTJSON_INVALID_UTF8_SUBSTITUTE
#define FASTJSON_ENCODE_THROW_ON_ERROR        (1 << 22)

/* Decode-side flag bits (subset of ext/json's decode flags). */
#define FASTJSON_DECODE_OBJECT_AS_ARRAY       (1 << 0)
#define FASTJSON_DECODE_BIGINT_AS_STRING      (1 << 1)
#define FASTJSON_DECODE_INVALID_UTF8_IGNORE   FASTJSON_INVALID_UTF8_IGNORE
#define FASTJSON_DECODE_INVALID_UTF8_SUBSTITUTE FASTJSON_INVALID_UTF8_SUBSTITUTE
#define FASTJSON_DECODE_THROW_ON_ERROR        (1 << 22)
/* fastjson-only: no ext/json counterpart. Tolerate the JSONC subset
 * (line and block comments, trailing commas, a leading UTF-8 BOM) that
 * ext/json rejects. Bit 23 sits just past ext/json's flag range so it
 * never collides with a JSON_* value a caller might OR in. */
#define FASTJSON_DECODE_RELAXED               (1 << 23)

/* Cached class entries resolved at MINIT. Both come from ext/json
 * (which is always loaded in standard PHP builds); we do not register
 * our own copies. NULL on builds where ext/json is somehow absent --
 * fastjson degrades gracefully (throws \Exception, ignores
 * JsonSerializable). */
extern zend_class_entry *fastjson_json_exception_ce;
extern zend_class_entry *fastjson_json_serializable_ce;

ZEND_BEGIN_MODULE_GLOBALS(fastjson)
    zend_long last_err_code;
    /* yyjson's err.msg is a string literal owned by yyjson.c; we store
     * the pointer directly without copying. NULL means "no error
     * recorded yet this request" -- last_error_msg() returns "No error"
     * for both NULL and the success state. */
    const char *last_err_msg;
    /* Source location of the most recent read error. last_err_pos is a
     * byte offset into the parsed input (-1 = none/not-applicable, e.g.
     * encode/IO/depth errors that have no source offset); last_err_line
     * and last_err_col are 1-based (0 = unknown). Populated by
     * fastjson_set_read_error via yyjson_locate_pos. */
    zend_long last_err_pos;
    zend_long last_err_line;
    zend_long last_err_col;
ZEND_END_MODULE_GLOBALS(fastjson)

ZEND_EXTERN_MODULE_GLOBALS(fastjson)

#define FASTJSON_G(v) ZEND_MODULE_GLOBALS_ACCESSOR(fastjson, v)

#include "yyjson.h"

typedef struct {
    zend_long code;
    const char *msg;
    zend_long pos;
    zend_long line;
    zend_long col;
} fastjson_error_state;

static zend_always_inline void fastjson_error_state_clear(
    fastjson_error_state *state)
{
    state->code = FASTJSON_ERROR_NONE;
    state->msg = NULL;
    state->pos = -1;
    state->line = 0;
    state->col = 0;
}

static zend_always_inline void fastjson_error_state_set(
    fastjson_error_state *state, zend_long code, const char *msg)
{
    state->code = code;
    state->msg = msg;
    state->pos = -1;
    state->line = 0;
    state->col = 0;
}

/* Direct-write encoder entry point (fastjson_directwrite.c).
 * Returns the encoded zend_string on success (caller owns it), or NULL on
 * unrecoverable error. `error_state` receives this invocation's outcome so
 * nested callbacks cannot replace the outer operation's final error. */
zend_string *fastjson_directwrite_encode(zval *value, zend_long flags,
                                         zend_long depth,
                                         fastjson_error_state *error_state);

/* Translate a yyjson read code into the matching ext/json JSON_ERROR_*
 * int. SUCCESS -> NONE; INVALID_STRING -> UTF8 (yyjson's signal for bad
 * UTF-8 inside a JSON string); everything else -> SYNTAX. */
zend_long fastjson_translate_read_code(yyjson_read_code yy);

/* Translate fastjson encode $flags to yyjson write flags: escape
 * slashes/unicode unless the UNESCAPED_* bits are set. When with_pretty
 * is true, PRETTY_PRINT maps to YYJSON_WRITE_PRETTY (yyjson-writer
 * callers); the direct-write encoder passes false and emits pretty
 * output itself. Defined in fastjson_directwrite.c; shared so
 * fastjson_pointer_set does not re-derive the mapping. */
yyjson_write_flag fastjson_translate_write_flags(zend_long php_flags,
                                                 bool with_pretty);

/* Reset module globals to JSON_ERROR_NONE / NULL. Called on every
 * successful fastjson_* call and on each RINIT. */
void fastjson_clear_error(void);

/* Record a FASTJSON_ERROR_* code directly (no read-code translation). msg may
 * be NULL or a string literal -- not freed. Clears the source-location fields
 * because encode, I/O, and depth errors have no source byte offset. */
void fastjson_set_error_code(zend_long code, const char *msg);

void fastjson_save_error_state(fastjson_error_state *state);
void fastjson_restore_error_state(const fastjson_error_state *state);

/* Snapshot global error state and (in non-throw mode) clear it so
 * argument-validation ValueErrors leave last_error as NONE rather than
 * whatever a previous call recorded. Used at the top of every PHP_FUNCTION
 * that participates in the JSON_THROW_ON_ERROR contract. */
static zend_always_inline void fastjson_throw_mode_init(
    bool throw_mode, fastjson_error_state *saved)
{
    fastjson_save_error_state(saved);
    if (!throw_mode) {
        fastjson_clear_error();
    }
}

void fastjson_throw_error(zend_long code, const char *msg,
                          const char *fallback_msg,
                          const fastjson_error_state *saved_err);
void fastjson_throw_current_error(const char *fallback_msg,
                                  const fastjson_error_state *saved_err);
void fastjson_throw_read_error(const yyjson_read_err *err,
                               const fastjson_error_state *saved_err);

/* Record a yyjson read error together with its source location. Stores
 * the translated code and (verbatim) message, and additionally captures
 * err->pos plus the 1-based line/column it maps to via yyjson_locate_pos
 * over [json, json_len). Use at any read-error site that still has the
 * source buffer in scope so callers can recover where a parse failed
 * through fastjson_last_error_pos/info. */
void fastjson_set_read_error(const char *json, size_t json_len,
                             const yyjson_read_err *err);

/* Returns true if `s` contains an unquoted Inf, Infinity, or NaN
 * literal token (case-insensitive). Used after the overflow-retry
 * parse to reject inputs that mix overflow with a literal that
 * ext/json rejects. */
bool fastjson_input_has_inf_nan_literal(const char *s, size_t len,
                                        bool allow_comments);

/* Returns true if the bytes starting at s[pos] form a valid UTF-8
 * sequence: ASCII, or a complete and well-formed 2/3/4-byte
 * sequence with no overlong, surrogate, or out-of-range encodings.
 * Used to gate the parse-error UTF-8 reclassification -- yyjson
 * reports any unexpected character as a syntax issue, but ext/json
 * distinguishes valid-UTF-8-but-not-JSON (SYNTAX) from
 * malformed-UTF-8 (UTF8). */
bool fastjson_byte_is_valid_utf8_start(const char *s, size_t len, size_t pos);

/* Side selector for fastjson_sanitize_utf8. ext/json's encoder and
 * decoder differ in two compatibility-relevant ways:
 *
 *   FJ_SAN_ENCODE: matches json_encode. When IGNORE and SUBSTITUTE
 *     are both set, IGNORE wins (invalid bytes dropped). On
 *     SUBSTITUTE alone, the advance-on-invalid count follows
 *     php_next_utf8_char's maximal-ill-formed-subpart rule (UTR-36
 *     strategy 2), so a malformed 4-byte sequence like F0 80 80 A
 *     produces ONE U+FFFD.
 *
 *   FJ_SAN_DECODE: matches json_decode. When IGNORE and SUBSTITUTE
 *     are both set, SUBSTITUTE wins. On SUBSTITUTE, every invalid
 *     byte that breaks the UTF-8 state machine produces ONE U+FFFD
 *     (so F0 80 80 yields THREE U+FFFDs).
 *
 * The asymmetry exists in ext/json itself, so fastjson mirrors it
 * to keep upstream tests passing byte-for-byte. */
typedef enum {
    FJ_SAN_ENCODE,
    FJ_SAN_DECODE,
} fj_sanitize_mode;

/* Sanitize a byte string under JSON_INVALID_UTF8_IGNORE /
 * JSON_INVALID_UTF8_SUBSTITUTE semantics. Returns a newly-emalloc'd
 * buffer; the caller must efree() it. The resulting length is
 * written to *out_len. */
char *fastjson_sanitize_utf8(const char *s, size_t len, zend_long flags,
                             fj_sanitize_mode mode, size_t *out_len);

/* Returns true if `s[0..len)` is well-formed UTF-8 by the same
 * per-codepoint rules fastjson_sanitize_utf8 uses to detect bad bytes
 * (no overlongs, no surrogates, no out-of-range, no truncated trails).
 * Single pass, no allocation. Callers use this on the IGNORE / SUBSTITUTE
 * decode path to skip the sanitize alloc+copy when the input is already
 * clean -- the common case for defensive callers that always pass the
 * flag. */
bool fastjson_utf8_well_formed(const char *s, size_t len);

typedef enum {
    FJ_STRING_SIZE_OK = 0,
    FJ_STRING_SIZE_INVALID_UTF8,
    FJ_STRING_SIZE_TOO_LARGE,
} fj_string_size_status;

/* Compute yyjson_write_string_to_buf()'s exact output length, including
 * quotes, while validating UTF-8. Used only for large strings so the writer
 * does not reserve its 6x worst case when the actual output is much smaller. */
fj_string_size_status fastjson_json_string_size(const char *s, size_t len,
                                                 yyjson_write_flag flags,
                                                 size_t *out_len,
                                                 bool *copyable_ascii);

/* Append one large JSON string to `buf` with exact capacity. On OK the
 * smart-string length is advanced; other statuses leave it unchanged. */
fj_string_size_status fastjson_write_large_json_string(
    smart_str *buf, const char *s, size_t len, yyjson_write_flag flags);
bool fastjson_apply_hex_escapes(smart_str *buf, zend_long flags,
                                size_t start_pos);

/* Returns true if the value of either UTF-8-handling flag bit is set
 * in `flags`. Folds the two-bit check the encoder/decoder do many
 * times into one named test so the IS_STRING hot path stays a
 * single comparison. */
#define FASTJSON_HAS_UTF8_HANDLING_FLAG(flags) \
    (((flags) & (FASTJSON_INVALID_UTF8_IGNORE \
                 | FASTJSON_INVALID_UTF8_SUBSTITUTE)) != 0)

#define FASTJSON_EXACT_STRING_THRESHOLD (256 * 1024)
#define FASTJSON_EXACT_NONASCII_THRESHOLD (1024 * 1024)
#define FASTJSON_ENCODE_HEX_MASK (FASTJSON_ENCODE_HEX_TAG \
    | FASTJSON_ENCODE_HEX_AMP | FASTJSON_ENCODE_HEX_APOS \
    | FASTJSON_ENCODE_HEX_QUOT)
#define FASTJSON_NUM_INT_WORST 24
#define FASTJSON_NUM_REAL_WORST 40

static zend_always_inline void fastjson_append_newline_indent(
    smart_str *buf, int level)
{
    if (UNEXPECTED(level > 8
            && (size_t)level <= (ZSTR_MAX_LEN - 1) / 4)) {
        size_t spaces = (size_t)level * 4;
        smart_str_alloc(buf, spaces + 1, 0);
        smart_str_appendc(buf, '\n');
        if (spaces != 0) {
            memset(ZSTR_VAL(buf->s) + ZSTR_LEN(buf->s), ' ', spaces);
            ZSTR_LEN(buf->s) += spaces;
        }
        return;
    }
    smart_str_appendc(buf, '\n');
    for (int i = 0; i < level; i++) {
        smart_str_appendl(buf, "    ", 4);
    }
}

typedef enum {
    FJ_SPLICE_OK = 0,
    FJ_SPLICE_SETTABLE_FAIL,
    FJ_SPLICE_WRITE_FAIL,
    FJ_SPLICE_DEPTH_FAIL,
    FJ_SPLICE_TOO_LARGE,
    FJ_SPLICE_INF_OR_NAN,
    FJ_SPLICE_INVALID_UTF8,
    FJ_SPLICE_AMBIGUOUS,
} fj_splice_status;

typedef struct {
    const char *str;
    size_t len;
} fj_ptr_seg;

typedef struct {
    fj_ptr_seg *segs;
    char *storage;
    size_t nsegs;
} fastjson_pointer_plan;

typedef struct {
    char *owned_str;
    zend_string *encoded;
    yyjson_val stack_val;
    const yyjson_val *repl;
} fastjson_pointer_repl;

bool fastjson_pointer_build_replacement(zval *value, zend_long value_flags,
                                        zend_long depth,
                                        fastjson_pointer_repl *out,
                                        fastjson_error_state *error_state);

/* Count RFC 6901 segments in a pointer ("" -> 0, "/a" -> 1, "/a/b" -> 2).
 * The replacement in a non-root set nests under this many path containers,
 * so the caller subtracts it from the value's depth budget. */
size_t fastjson_pointer_count_segments(const char *ptr, size_t len);

bool fastjson_pointer_plan_init(yyjson_val *root, const char *pointer,
                                size_t pointer_len, size_t depth_limit,
                                fastjson_pointer_plan *plan,
                                fj_splice_status *status);
void fastjson_pointer_plan_destroy(fastjson_pointer_plan *plan);
fj_splice_status fastjson_pointer_resolve(yyjson_val *root,
                                          const char *pointer,
                                          size_t pointer_len,
                                          yyjson_val **target);

zend_string *fastjson_imut_pointer_set_write(yyjson_val *root,
                                             const fastjson_pointer_plan *plan,
                                             const fastjson_pointer_repl *replacement,
                                             zend_long flags,
                                             size_t depth_limit,
                                             fj_splice_status *status);

#endif /* PHP_FASTJSON_H */
