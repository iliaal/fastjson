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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <inttypes.h>
#include <limits.h>
#include <stdio.h>

#include "php.h"
#include "php_streams.h"
#include "ext/standard/file.h"
#if PHP_VERSION_ID >= 80300 && defined(ZEND_CHECK_STACK_LIMIT)
#include "Zend/zend_call_stack.h"
#else
/* zend_call_stack_overflowed() / EG(stack_limit) are 8.3+, and even on
 * 8.3+ the function is only declared when ZEND_CHECK_STACK_LIMIT is
 * configured (php-src cannot detect stack bounds on every platform).
 * Where either is absent, the remaining_depth counter (default 512)
 * still bounds recursion, so the secondary C-stack check degrades to a
 * no-op. */
#define zend_call_stack_overflowed(limit) (0)
#endif
#include "Zend/zend_exceptions.h"
#include "Zend/zend_smart_str.h"
#include "php_fastjson.h"
#include "fastjson_arginfo.h"
#include "fastjson_alloc.h"
#include "yyjson.h"

/* Validate the $depth argument exactly as ext/json does: <= 0 and
 * > INT_MAX both raise a ValueError on argument `argno`. Every fastjson_*
 * function that takes a $depth uses this so the message and bounds stay
 * identical across them. Expands a RETURN_THROWS(), so it may only appear
 * inside a PHP_FUNCTION. */
#define FASTJSON_VALIDATE_DEPTH(depth, argno) do { \
    if ((depth) <= 0) { \
        zend_argument_value_error((argno), "must be greater than 0"); \
        RETURN_THROWS(); \
    } \
    if ((depth) > INT_MAX) { \
        zend_argument_value_error((argno), "must be less than %d", INT_MAX); \
        RETURN_THROWS(); \
    } \
} while (0)

/* Secondary helpers below call recursive C code before the normal zval
 * walker can enforce the public $depth. Keep their effective depth bounded
 * even when the caller passes a very large value. */
#define FASTJSON_STACK_DEPTH_LIMIT 1024

static size_t fastjson_effective_stack_depth(zend_long depth)
{
    if (depth > FASTJSON_STACK_DEPTH_LIMIT) {
        return FASTJSON_STACK_DEPTH_LIMIT;
    }
    return (size_t)depth;
}

#define FASTJSON_POINTER_VALUE_ENCODE_FLAGS ( \
    FASTJSON_ENCODE_HEX_TAG | \
    FASTJSON_ENCODE_HEX_AMP | \
    FASTJSON_ENCODE_HEX_APOS | \
    FASTJSON_ENCODE_HEX_QUOT | \
    FASTJSON_ENCODE_FORCE_OBJECT | \
    FASTJSON_ENCODE_NUMERIC_CHECK | \
    FASTJSON_ENCODE_UNESCAPED_SLASHES | \
    FASTJSON_ENCODE_PRETTY_PRINT | \
    FASTJSON_ENCODE_UNESCAPED_UNICODE | \
    FASTJSON_ENCODE_PARTIAL_OUTPUT_ON_ERROR | \
    FASTJSON_ENCODE_PRESERVE_ZERO_FRACTION | \
    FASTJSON_ENCODE_INVALID_UTF8_IGNORE | \
    FASTJSON_ENCODE_INVALID_UTF8_SUBSTITUTE | \
    FASTJSON_ENCODE_THROW_ON_ERROR)

static size_t fj_write_uint64_dec(char *buf, uint64_t u)
{
    char tmp[24];
    size_t n = 0;
    do {
        tmp[n++] = (char)('0' + (u % 10));
        u /= 10;
    } while (u != 0);
    for (size_t i = 0; i < n; i++) {
        buf[i] = tmp[n - 1 - i];
    }
    return n;
}

static size_t fj_write_sint64_dec(char *buf, int64_t s)
{
    if (s < 0) {
        *buf++ = '-';
        uint64_t u = (uint64_t)(-(s + 1)) + 1;
        return 1 + fj_write_uint64_dec(buf, u);
    }
    return fj_write_uint64_dec(buf, (uint64_t)s);
}

/* yyjson NUM -> zval. Real -> double; uint <= INT64_MAX -> long; a
 * larger uint becomes a decimal string under JSON_BIGINT_AS_STRING and
 * otherwise widens to double (matching ext/json, which never wraps a
 * uint past LONG_MAX into a negative long); sint -> long. Shared by
 * both decode walkers and force-inlined so the hot path stays
 * call-free. */
static zend_always_inline void fj_num_to_zval(yyjson_val *val,
                                              zend_long flags, zval *out)
{
    if (yyjson_is_real(val)) {
        ZVAL_DOUBLE(out, yyjson_get_real(val));
    } else if (yyjson_is_uint(val)) {
        uint64_t u = yyjson_get_uint(val);
        if (u <= (uint64_t)ZEND_LONG_MAX) {
            ZVAL_LONG(out, (zend_long)u);
        } else if (flags & FASTJSON_DECODE_BIGINT_AS_STRING) {
            char buf[24];
            size_t len = fj_write_uint64_dec(buf, u);
            ZVAL_STRINGL(out, buf, len);
        } else {
            ZVAL_DOUBLE(out, (double)u);
        }
    } else { /* SINT */
        int64_t s = yyjson_get_sint(val);
        if (s >= ZEND_LONG_MIN) {
            ZVAL_LONG(out, (zend_long)s);
        } else if (flags & FASTJSON_DECODE_BIGINT_AS_STRING) {
            /* On 32-bit zend_long builds a negative int64 below
             * ZEND_LONG_MIN would truncate; mirror the uint branch and
             * stringify (or widen to double). Constant-false and elided
             * on 64-bit, where ZEND_LONG_MIN == INT64_MIN. */
            char buf[24];
            size_t len = fj_write_sint64_dec(buf, s);
            ZVAL_STRINGL(out, buf, len);
        } else {
            ZVAL_DOUBLE(out, (double)s);
        }
    }
}

/* yyjson RAW -> zval. yyjson emits YYJSON_TYPE_RAW under
 * YYJSON_READ_BIGNUM_AS_RAW for two distinct cases:
 *   1. Big integers that overflow int64/uint64. ext/json's
 *      JSON_BIGINT_AS_STRING wants these as strings; passthrough.
 *   2. Floats whose exponent overflows a double (e.g. 1e309).
 *      ext/json decodes those to PHP INF -- BIGINT_AS_STRING does NOT
 *      apply to floats. Re-parse via strtod so we return the same
 *      float ext/json would.
 * Float shape: presence of '.', 'e', or 'E' in the raw text. Shared by
 * both decode walkers and force-inlined to keep the hot path call-free. */
static zend_always_inline void fj_raw_to_zval(yyjson_val *val, zval *out)
{
    const char *raw = yyjson_get_raw(val);
    size_t raw_len = yyjson_get_len(val);
    bool is_float = false;
    for (size_t i = 0; i < raw_len; i++) {
        char c = raw[i];
        if (c == '.' || c == 'e' || c == 'E') { is_float = true; break; }
    }
    if (is_float) {
        ZVAL_DOUBLE(out, zend_strtod(raw, NULL));
    } else {
        ZVAL_STRINGL(out, raw, raw_len);
    }
}

/* Recursive walker: yyjson_val -> zval. Returns true on success, false
 * on FASTJSON_ERROR_DEPTH (the only mid-walk failure mode). On failure
 * `out` is left as a partial zend_array/zend_object whose
 * zval_ptr_dtor in the caller's RETURN_NULL path frees the partially
 * built children correctly.
 *
 * Ownership: every refcounted zval written into `out` is owned by
 * `out` -- callers transfer it into a HashTable bucket via
 * zend_hash_*_insert/update which copies the zval bytes (refcount
 * unchanged), or into return_value via RETVAL_*. The local in this
 * function then goes out of scope; the bucket / return_value owns
 * the value from that point.
 *
 * remaining_depth follows ext/json's convention: starts at the
 * caller's $depth, decrements before recursing into ARR/OBJ children.
 * A top-level scalar at remaining_depth=1 is allowed; a top-level
 * scalar at remaining_depth=0 fails. */
/* The recursive yyjson_val -> zval walker is generated twice from
 * fastjson_walk.inc: fastjson_yyval_to_zval (fast, no UTF-8 handling)
 * and fastjson_yyval_to_zval_sanitize (runs fastjson_sanitize_utf8 on
 * malformed string values and object keys, invoked only when the caller
 * set JSON_INVALID_UTF8_IGNORE / SUBSTITUTE). The template's
 * FJ_WALK_SANITIZE is a preprocessor literal, so the fast instantiation
 * carries none of the sanitize code -- two branch-free specialized
 * functions, one source of truth for the shared scaffolding. */
#define FJ_WALK_NAME fastjson_yyval_to_zval
#define FJ_WALK_SANITIZE 0
#include "fastjson_walk.inc"
#undef FJ_WALK_NAME
#undef FJ_WALK_SANITIZE

#define FJ_WALK_NAME fastjson_yyval_to_zval_sanitize
#define FJ_WALK_SANITIZE 1
#include "fastjson_walk.inc"
#undef FJ_WALK_NAME
#undef FJ_WALK_SANITIZE

/* Read a JSON buffer into an immutable yyjson doc, applying fastjson's
 * decode-flag translation, the ext/json inf/nan exponent-overflow retry,
 * and the parse-error UTF-8 reclassification. Returns the doc (caller
 * frees with yyjson_doc_free) or NULL with *err populated; err->code is
 * already mapped to the right yyjson_read_code bucket. Shared by
 * fastjson_decode, fastjson_pointer_get, and fastjson_merge_patch. */
static yyjson_doc *fastjson_read_doc(const char *json, size_t json_len,
                                     zend_long flags, yyjson_read_err *err)
{
    /* BIGINT_AS_STRING -> BIGNUM_AS_RAW so numbers that overflow
     * uint64/double surface as YYJSON_TYPE_RAW (raw digit string); the
     * walker stringifies those. */
    yyjson_read_flag yflags = 0;
    if (flags & FASTJSON_DECODE_BIGINT_AS_STRING) {
        yflags |= YYJSON_READ_BIGNUM_AS_RAW;
    }
    /* IGNORE or SUBSTITUTE: accept invalid UTF-8 in strings; the
     * sanitizing walker strips/replaces those bytes afterwards. */
    if (flags & (FASTJSON_DECODE_INVALID_UTF8_IGNORE
                 | FASTJSON_DECODE_INVALID_UTF8_SUBSTITUTE)) {
        yflags |= YYJSON_READ_ALLOW_INVALID_UNICODE;
    }
    /* RELAXED: tolerate the JSONC subset (comments, trailing commas, BOM). */
    if (flags & FASTJSON_DECODE_RELAXED) {
        yflags |= YYJSON_READ_ALLOW_COMMENTS
                | YYJSON_READ_ALLOW_TRAILING_COMMAS
                | YYJSON_READ_ALLOW_BOM;
    }

    yyjson_doc *doc = yyjson_read_opts((char *)json, json_len, yflags,
                                       &fastjson_php_alc, err);
    if (doc == NULL && err->msg
            && strcmp(err->msg, "number is infinity when parsed as double") == 0
            && !fastjson_input_has_inf_nan_literal(json, json_len,
                    (flags & FASTJSON_DECODE_RELAXED) != 0)) {
        /* yyjson rejects exponent-overflow numbers like "1e309"; ext/json
         * decodes them to INF. Retry with ALLOW_INF_AND_NAN to match,
         * unless the input also carries an unquoted Inf/NaN literal token
         * (which ext/json rejects). */
        doc = yyjson_read_opts((char *)json, json_len,
                               yflags | YYJSON_READ_ALLOW_INF_AND_NAN,
                               &fastjson_php_alc, err);
    }
    if (doc == NULL
            && err->pos < json_len
            && (unsigned char)json[err->pos] >= 0x80
            && !fastjson_byte_is_valid_utf8_start(json, json_len, err->pos)) {
        /* ext/json reports a malformed UTF-8 byte at the error position as
         * JSON_ERROR_UTF8; a valid-UTF-8-but-not-JSON byte stays SYNTAX.
         * yyjson lumps both as UNEXPECTED_CHARACTER, so reclassify only
         * genuinely malformed UTF-8. */
        err->code = YYJSON_READ_ERROR_INVALID_STRING;
    }
    return doc;
}

/* Shared decode core for fastjson_decode and fastjson_file_decode.
 * Runs the yyjson read (with the inf/nan-overflow retry and parse-error
 * UTF-8 reclassification), dispatches the appropriate walker into
 * `return_value`, and honors the JSON_THROW_ON_ERROR state-preservation
 * contract. $depth must already be validated and, in non-throw mode,
 * the caller must already have cleared the error state; saved_err is the
 * snapshot to restore on the throw path. The param is named
 * `return_value` so RETURN_NULL() / RETURN_THROWS() expand correctly. */
static void fastjson_decode_into(const char *json, size_t json_len,
                                 bool use_assoc, zend_long depth,
                                 zend_long flags, bool throw_mode,
                                 const fastjson_error_state *saved_err,
                                 zval *return_value)
{
    yyjson_read_err err;
    yyjson_doc *doc = fastjson_read_doc(json, json_len, flags, &err);
    if (doc == NULL) {
        if (throw_mode) {
            fastjson_throw_read_error(&err, saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(json, json_len, &err);
        RETURN_NULL();
    }

    yyjson_val *root = yyjson_doc_get_root(doc);
    /* Single dispatch: pick the sanitizing walker only when the
     * caller asked for UTF-8 handling. The fast walker is otherwise
     * branch-identical to the pre-IGNORE/SUBSTITUTE code. */
    bool walk_ok = FASTJSON_HAS_UTF8_HANDLING_FLAG(flags)
        ? fastjson_yyval_to_zval_sanitize(root, use_assoc, depth, flags, return_value)
        : fastjson_yyval_to_zval(root, use_assoc, depth, flags, return_value);
    if (!walk_ok) {
        /* fastjson_yyval_to_zval already populated FASTJSON_ERROR_DEPTH.
         * return_value may hold a partial zend_array/zend_object; reset
         * so RETURN_NULL gives the user a clean null. */
        zval_ptr_dtor(return_value);
        ZVAL_NULL(return_value);
        yyjson_doc_free(doc);
        if (throw_mode) {
            fastjson_throw_current_error("Maximum stack depth exceeded",
                                         saved_err);
            RETURN_THROWS();
        }
        return;
    }

    yyjson_doc_free(doc);
    /* Match ext/json's THROW_ON_ERROR contract: when the caller opted
     * into exceptions for errors, the global error state is the
     * caller's previous state, not "no error". Only clear on the
     * non-throw success path. */
    if (!throw_mode) {
        fastjson_clear_error();
    }
}

PHP_FUNCTION(fastjson_decode)
{
    char *json;
    size_t json_len;
    bool assoc = false;
    bool assoc_is_null = true;
    zend_long depth = 512;
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(1, 4)
        Z_PARAM_STRING(json, json_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL_OR_NULL(assoc, assoc_is_null)
        Z_PARAM_LONG(depth)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    /* JSON_THROW_ON_ERROR contract: on error, throw a JsonException
     * and leave the GLOBAL fastjson_last_error state unchanged. This
     * is how ext/json behaves (json_exceptions_error_clearing.phpt).
     * We snapshot global state at entry and restore it on the throw
     * path. The success path clears as usual on entry; non-throw
     * failure path persists the error normally. */
    bool throw_mode = (flags & FASTJSON_DECODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);

    /* In non-throw mode, ext/json clears the error state up front so
     * any argument-validation ValueError leaves last_error as NONE
     * rather than whatever a previous call recorded. THROW_ON_ERROR
     * mode preserves the prior state per its own contract. */
    if (!throw_mode) {
        fastjson_clear_error();
    }

    /* Match ext/json's argument validation contract verbatim:
     *   depth <= 0          -> ValueError "must be greater than 0"
     *   depth > INT_MAX     -> ValueError "must be less than %d"
     * Both are arg #3 (after $json, $associative). */
    FASTJSON_VALIDATE_DEPTH(depth, 3);

    /* ext/json contract: explicit $associative wins; when null, the
     * JSON_OBJECT_AS_ARRAY bit in $flags pivots. Defaults to stdClass
     * when neither is set. */
    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    fastjson_decode_into(json, json_len, use_assoc, depth, flags,
                         throw_mode, &saved_err,
                         return_value);
}

PHP_FUNCTION(fastjson_file_decode)
{
    char *path;
    size_t path_len;
    bool assoc = false;
    bool assoc_is_null = true;
    zend_long depth = 512;
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(1, 4)
        Z_PARAM_PATH(path, path_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL_OR_NULL(assoc, assoc_is_null)
        Z_PARAM_LONG(depth)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    bool throw_mode = (flags & FASTJSON_DECODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);

    if (!throw_mode) {
        fastjson_clear_error();
    }

    /* $depth is arg #3 here too ($filename, $associative, $depth). */
    FASTJSON_VALIDATE_DEPTH(depth, 3);

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    /* Read through the streams layer (wrappers + open_basedir honored).
     * No REPORT_ERRORS, so a missing/unreadable file emits no warning --
     * that failure is surfaced via the null return + last_error, matching
     * the documented contract. The one exception is an open_basedir denial:
     * the plain-file wrapper emits its own open_basedir warning regardless
     * of REPORT_ERRORS (exactly as file_get_contents does), which we let
     * through rather than suppress -- a security-boundary violation should
     * stay visible. An I/O failure is NOT a JSON
     * error, so it never throws even under JSON_THROW_ON_ERROR: there
     * is no JSON_ERROR_* code for a filesystem fault, and a missing
     * file should not masquerade as a JsonException. We surface it as
     * FASTJSON_ERROR_SYNTAX (the closest bucket: "no valid JSON could
     * be obtained from the source") with a descriptive message. */
    /* php_stream_context_from_zval(NULL, 0) resolves the request's default
     * stream context (allocating it lazily), exactly as file_get_contents
     * does -- so options set via stream_context_set_default() and user
     * wrappers relying on them are honored. */
    php_stream_context *context = php_stream_context_from_zval(NULL, 0);
    php_stream *stream = php_stream_open_wrapper_ex(path, "rb", 0, NULL,
                                                    context);
    if (stream == NULL) {
        /* A userspace stream wrapper's open() may have thrown; propagate
         * that exception rather than masking it with our own SYNTAX error
         * (and never RETURN_NULL with an exception still pending). */
        if (EG(exception)) {
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "Failed to open file for reading");
        RETURN_NULL();
    }
    zend_string *contents = php_stream_copy_to_mem(stream,
                                                   PHP_STREAM_COPY_ALL, 0);
    bool reached_eof = php_stream_eof(stream);
    /* php_stream_copy_to_mem returns NULL for a zero-byte stream, not an
     * empty zend_string. An empty file is not an I/O failure -- it must
     * decode exactly like fastjson_decode(""): yyjson rejects empty input
     * as a syntax error and JSON_THROW_ON_ERROR throws. Distinguish it
     * from a genuine read error by the stream's EOF state (checked before
     * close). */
    if (contents == NULL && reached_eof) {
        contents = ZSTR_EMPTY_ALLOC();
    }
    php_stream_close(stream);
    if (contents == NULL || !reached_eof) {
        /* A userspace wrapper's read() may have thrown mid-copy; propagate
         * it rather than clobbering it with a SYNTAX error. */
        if (contents != NULL) {
            zend_string_release(contents);
        }
        if (EG(exception)) {
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "Failed to read file");
        RETURN_NULL();
    }

    fastjson_decode_into(ZSTR_VAL(contents), ZSTR_LEN(contents), use_assoc,
                         depth, flags, throw_mode, &saved_err, return_value);
    zend_string_release(contents);
}

/* Convert a resolved/merged immutable subtree into return_value, mirroring
 * fastjson_decode_into's walker dispatch and depth-failure handling. Frees
 * `doc`. Returns true on success (caller clears error on the non-throw
 * path), false if the walk hit the depth cap (return_value left NULL,
 * exception thrown under throw_mode). */
static bool fastjson_walk_doc_into(yyjson_doc *doc, yyjson_val *root,
                                   bool use_assoc, zend_long depth,
                                   zend_long flags, bool throw_mode,
                                   const fastjson_error_state *saved_err,
                                   zval *return_value)
{
    bool walk_ok = FASTJSON_HAS_UTF8_HANDLING_FLAG(flags)
        ? fastjson_yyval_to_zval_sanitize(root, use_assoc, depth, flags, return_value)
        : fastjson_yyval_to_zval(root, use_assoc, depth, flags, return_value);
    if (!walk_ok) {
        /* The walker set FASTJSON_ERROR_DEPTH and return_value may hold a
         * partial container; reset to a clean null. */
        zval_ptr_dtor(return_value);
        ZVAL_NULL(return_value);
        yyjson_doc_free(doc);
        if (throw_mode) {
            fastjson_throw_current_error("Maximum stack depth exceeded",
                                         saved_err);
        }
        return false;
    }
    yyjson_doc_free(doc);
    return true;
}

PHP_FUNCTION(fastjson_pointer_get)
{
    char *json, *pointer;
    size_t json_len, pointer_len;
    bool assoc = false;
    bool assoc_is_null = true;
    zend_long depth = 512;
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(2, 5)
        Z_PARAM_STRING(json, json_len)
        Z_PARAM_STRING(pointer, pointer_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL_OR_NULL(assoc, assoc_is_null)
        Z_PARAM_LONG(depth)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    bool throw_mode = (flags & FASTJSON_DECODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);
    if (!throw_mode) {
        fastjson_clear_error();
    }

    FASTJSON_VALIDATE_DEPTH(depth, 4);

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    yyjson_read_err err;
    yyjson_doc *doc = fastjson_read_doc(json, json_len, flags, &err);
    if (doc == NULL) {
        if (throw_mode) {
            fastjson_throw_read_error(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(json, json_len, &err);
        RETURN_NULL();
    }

    /* RFC 6901 JSON Pointer. yyjson_ptr_getn returns NULL when the
     * pointer does not resolve OR is malformed (e.g. missing leading
     * '/'); both are treated as "no value" -- not a JSON error -- so we
     * return null with the error state left clear. A pointer that
     * resolves to a JSON null returns null too (target != NULL); the two
     * are indistinguishable from PHP, consistent with the decode family's
     * documented null-ambiguity. The empty pointer "" selects the whole
     * document per RFC 6901. */
    yyjson_val *target = yyjson_ptr_getn(yyjson_doc_get_root(doc),
                                         pointer, pointer_len);
    if (target == NULL) {
        yyjson_doc_free(doc);
        /* Absent/malformed pointer is not a JSON error. The stub documents
         * this as null + FASTJSON_ERROR_NONE, so clear here even under
         * THROW_ON_ERROR (which skipped the entry clear to preserve prior
         * state for a parse-error throw) rather than leak a stale error. */
        fastjson_clear_error();
        RETURN_NULL();
    }

    if (!fastjson_walk_doc_into(doc, target, use_assoc, depth, flags,
                                throw_mode, &saved_err, return_value)) {
        if (throw_mode) {
            RETURN_THROWS();
        }
        return;
    }
    if (!throw_mode) {
        fastjson_clear_error();
    }
}

/* True if `root` nests at least `limit` containers deep. Walks the
 * PARSED immutable tree iteratively (explicit stack, no C recursion),
 * returning as soon as the limit is reached so the work stack never
 * holds more than `limit` frames.
 *
 * fastjson_merge_patch needs this because yyjson_merge_patch and
 * yyjson_mut_doc_imut_copy recurse once per nesting level on the C stack
 * BEFORE the depth-capped zval walker runs; an adversarial deeply-nested
 * operand would overflow the stack and crash. Measuring depth on the
 * parsed tree (not the raw source bytes) is immune to comments, quotes,
 * and escapes -- a textual brace-counting scan has to replicate yyjson's
 * full lexer to stay correct, and under RELAXED a stray quote or brace
 * inside a comment silently corrupts the count. Counting matches the
 * walker's: the root container is depth 1, so reject when depth >= the
 * caller's $depth (json_decode("[[[1]]]", true, 3) -> NULL: three nested
 * containers need depth 4). */
typedef struct {
    bool is_obj;
    union {
        yyjson_arr_iter arr;
        yyjson_obj_iter obj;
    } it;
} fj_depth_frame;

static bool fastjson_val_depth_reaches(yyjson_val *root, size_t limit)
{
    if (limit == 0) {
        return true;
    }
    if (!yyjson_is_ctn(root)) {
        return false; /* scalar root: zero containers deep */
    }
    if (limit == 1) {
        return true; /* the root container alone is depth 1 */
    }

    size_t cap = 16, top = 0, depth = 1;
    fj_depth_frame *stack = emalloc(cap * sizeof(*stack));
    bool reached = false;

    stack[0].is_obj = yyjson_is_obj(root);
    if (stack[0].is_obj) {
        yyjson_obj_iter_init(root, &stack[0].it.obj);
    } else {
        yyjson_arr_iter_init(root, &stack[0].it.arr);
    }

    while (true) {
        fj_depth_frame *f = &stack[top];
        yyjson_val *child;
        if (f->is_obj) {
            yyjson_val *key = yyjson_obj_iter_next(&f->it.obj);
            child = key ? yyjson_obj_iter_get_val(key) : NULL;
        } else {
            child = yyjson_arr_iter_next(&f->it.arr);
        }
        if (child == NULL) {
            if (top == 0) break;
            top--;
            depth--;
            continue;
        }
        if (!yyjson_is_ctn(child)) {
            continue;
        }
        if (depth + 1 >= limit) {
            reached = true;
            break;
        }
        if (++top == cap) {
            cap *= 2;
            stack = erealloc(stack, cap * sizeof(*stack));
            f = NULL; /* invalidated by realloc; not used past here */
        }
        depth++;
        stack[top].is_obj = yyjson_is_obj(child);
        if (stack[top].is_obj) {
            yyjson_obj_iter_init(child, &stack[top].it.obj);
        } else {
            yyjson_arr_iter_init(child, &stack[top].it.arr);
        }
    }

    efree(stack);
    return reached;
}

PHP_FUNCTION(fastjson_merge_patch)
{
    char *target, *patch;
    size_t target_len, patch_len;
    bool assoc = false;
    bool assoc_is_null = true;
    zend_long depth = 512;
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(2, 5)
        Z_PARAM_STRING(target, target_len)
        Z_PARAM_STRING(patch, patch_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_BOOL_OR_NULL(assoc, assoc_is_null)
        Z_PARAM_LONG(depth)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    bool throw_mode = (flags & FASTJSON_DECODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);
    if (!throw_mode) {
        fastjson_clear_error();
    }

    FASTJSON_VALIDATE_DEPTH(depth, 4);

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    /* Parse both operands as immutable docs first; either failing is a
     * JSON error surfaced through the usual path. */
    yyjson_read_err err;
    yyjson_doc *tdoc = fastjson_read_doc(target, target_len, flags, &err);
    if (tdoc == NULL) {
        if (throw_mode) {
            fastjson_throw_read_error(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(target, target_len, &err);
        RETURN_NULL();
    }
    yyjson_doc *pdoc = fastjson_read_doc(patch, patch_len, flags, &err);
    if (pdoc == NULL) {
        yyjson_doc_free(tdoc);
        if (throw_mode) {
            fastjson_throw_read_error(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(patch, patch_len, &err);
        RETURN_NULL();
    }

    /* The merge and imut-copy below recurse on the C stack per nesting
     * level, ahead of the depth-capped walker. Reject operands whose
     * parsed nesting reaches the effective stack depth up front so a
     * pathological input with a huge user $depth fails with the same
     * FASTJSON_ERROR_DEPTH the walker would raise, rather than overflowing
     * the stack. A non-object patch replaces the target wholesale, so the
     * target cannot contribute to recursive merge/copy cost on that path. */
    size_t stack_depth = fastjson_effective_stack_depth(depth);
    yyjson_val *target_root = yyjson_doc_get_root(tdoc);
    yyjson_val *patch_root = yyjson_doc_get_root(pdoc);
    bool patch_merges_target = yyjson_is_obj(patch_root);
    if ((patch_merges_target
                && fastjson_val_depth_reaches(target_root, stack_depth))
            || fastjson_val_depth_reaches(patch_root, stack_depth)) {
        yyjson_doc_free(tdoc);
        yyjson_doc_free(pdoc);
        if (throw_mode) {
            fastjson_throw_error(FASTJSON_ERROR_DEPTH,
                                 "Maximum stack depth exceeded", NULL,
                                 &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                "Maximum stack depth exceeded");
        RETURN_NULL();
    }

    /* RFC 7386 JSON Merge Patch: a non-object patch replaces the target
     * wholesale; a null member deletes the corresponding key. The result
     * is built in a mutable doc, then copied to an immutable doc so the
     * shared yyjson_val -> zval walker can materialize it -- keeping a
     * single serialization-free path to a PHP value (callers re-encode
     * with fastjson_encode for byte-consistent output). */
    yyjson_doc *idoc = NULL;
    yyjson_mut_doc *mdoc = yyjson_mut_doc_new(&fastjson_php_alc);
    if (mdoc != NULL) {
        yyjson_mut_val *merged = yyjson_merge_patch(
            mdoc, target_root, patch_root);
        if (merged != NULL) {
            yyjson_mut_doc_set_root(mdoc, merged);
            idoc = yyjson_mut_doc_imut_copy(mdoc, &fastjson_php_alc);
        }
        yyjson_mut_doc_free(mdoc);
    }
    yyjson_doc_free(tdoc);
    yyjson_doc_free(pdoc);

    if (idoc == NULL) {
        /* Only reachable on allocator failure inside yyjson. */
        if (throw_mode) {
            fastjson_throw_error(FASTJSON_ERROR_SYNTAX,
                                 "fastjson_merge_patch failed", NULL,
                                 &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "fastjson_merge_patch failed");
        RETURN_NULL();
    }

    if (!fastjson_walk_doc_into(idoc, yyjson_doc_get_root(idoc), use_assoc,
                                depth, flags, throw_mode, &saved_err,
                                return_value)) {
        if (throw_mode) {
            RETURN_THROWS();
        }
        return;
    }
    if (!throw_mode) {
        fastjson_clear_error();
    }
}

PHP_FUNCTION(fastjson_pointer_exists)
{
    char *json, *pointer;
    size_t json_len, pointer_len;
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(2, 3)
        Z_PARAM_STRING(json, json_len)
        Z_PARAM_STRING(pointer, pointer_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    bool throw_mode = (flags & FASTJSON_DECODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);
    if (!throw_mode) {
        fastjson_clear_error();
    }

    yyjson_read_err err;
    yyjson_doc *doc = fastjson_read_doc(json, json_len, flags, &err);
    if (doc == NULL) {
        if (throw_mode) {
            fastjson_throw_read_error(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(json, json_len, &err);
        RETURN_FALSE;
    }

    /* yyjson_ptr_getn returns the resolved value or NULL (missing path or
     * malformed pointer). Unlike fastjson_pointer_get, the bool result is
     * unambiguous: a path resolving to JSON null still returns true. The
     * error state stays clear on a successful parse, so a false return with
     * last_error() == NONE means "absent", while a false return with an
     * error set means the JSON itself was malformed. */
    yyjson_val *target = yyjson_ptr_getn(yyjson_doc_get_root(doc),
                                         pointer, pointer_len);
    yyjson_doc_free(doc);
    if (target == NULL) {
        /* Absent/malformed pointer is the documented false + NONE signal
         * (fastjson.stub.php); clear even under THROW_ON_ERROR so a prior
         * error doesn't masquerade as "the JSON was malformed". */
        fastjson_clear_error();
    }
    RETURN_BOOL(target != NULL);
}

PHP_FUNCTION(fastjson_pointer_set)
{
    char *json, *pointer;
    size_t json_len, pointer_len;
    zval *value;
    zend_long flags = 0;
    zend_long depth = 512;

    ZEND_PARSE_PARAMETERS_START(3, 5)
        Z_PARAM_STRING(json, json_len)
        Z_PARAM_STRING(pointer, pointer_len)
        Z_PARAM_ZVAL(value)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(depth)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    bool throw_mode = (flags & FASTJSON_DECODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);
    if (!throw_mode) {
        fastjson_clear_error();
    }

    FASTJSON_VALIDATE_DEPTH(depth, 4);
    size_t stack_depth = fastjson_effective_stack_depth(depth);

    /* Parse the target document. RELAXED is the only decode-only bit accepted
     * by pointer_set: the document is written back out rather than
     * materialized into PHP, and low ext/json decode bits overlap encode bits.
     * UTF-8 handling applies to the replacement value only; invalid UTF-8 in
     * the base document is rejected so the writer never has to re-emit it. */
    yyjson_read_err err;
    yyjson_doc *idoc = fastjson_read_doc(json, json_len,
        flags & FASTJSON_DECODE_RELAXED, &err);
    if (idoc == NULL) {
        if (throw_mode) {
            fastjson_throw_read_error(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(json, json_len, &err);
        RETURN_FALSE;
    }

    /* The splice writer recurses on the C stack per container while
     * re-emitting the target. Root replacement discards the target after
     * parsing it, so only non-root edits need the target depth prewalk. */
    if (pointer_len != 0
            && fastjson_val_depth_reaches(yyjson_doc_get_root(idoc),
                                          stack_depth)) {
        yyjson_doc_free(idoc);
        if (throw_mode) {
            fastjson_throw_error(FASTJSON_ERROR_DEPTH,
                                 "Maximum stack depth exceeded", NULL,
                                 &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                "Maximum stack depth exceeded");
        RETURN_FALSE;
    }

    zend_long value_flags = flags & FASTJSON_POINTER_VALUE_ENCODE_FLAGS;

    /* The replacement lands nsegs containers deep in the output, so its own
     * nesting budget is what remains of the effective stack depth after the
     * pointer path. Passing the full depth let pointer_set emit a document
     * deeper than $depth -- one it would then reject on decode. Subtract the
     * path (floored at 0; a scalar replacement still encodes at budget 0). */
    size_t nsegs = fastjson_pointer_count_segments(pointer, pointer_len);
    zend_long repl_depth = (zend_long)stack_depth - (zend_long)nsegs - 1;
    if (repl_depth < 0) {
        repl_depth = 0;
    }

    fastjson_pointer_repl repl;
    if (!fastjson_pointer_build_replacement(value, value_flags,
                                            repl_depth, &repl)) {
        yyjson_doc_free(idoc);
        if (EG(exception)) {
            RETURN_THROWS();
        }
        if (throw_mode) {
            fastjson_throw_current_error("fastjson_pointer_set encode failed",
                                         &saved_err);
            RETURN_THROWS();
        }
        RETURN_FALSE;
    }

    fj_splice_status splice_status;
    zend_string *out = fastjson_imut_pointer_set_write(
        yyjson_doc_get_root(idoc), pointer, pointer_len, repl.repl, flags,
        stack_depth, &splice_status);

    if (repl.owned_str != NULL) {
        efree(repl.owned_str);
        repl.owned_str = NULL;
    }
    if (repl.doc != NULL) {
        yyjson_doc_free(repl.doc);
        repl.doc = NULL;
    }
    yyjson_doc_free(idoc);

    if (out == NULL) {
        if (splice_status == FJ_SPLICE_DEPTH_FAIL) {
            if (throw_mode) {
                fastjson_throw_error(FASTJSON_ERROR_DEPTH,
                                     "Maximum stack depth exceeded", NULL,
                                     &saved_err);
                RETURN_THROWS();
            }
            fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                    "Maximum stack depth exceeded");
            RETURN_FALSE;
        }
        if (splice_status == FJ_SPLICE_TOO_LARGE) {
            if (throw_mode) {
                fastjson_throw_error(FASTJSON_ERROR_UNSUPPORTED_TYPE,
                                     "Encoded JSON string is too large", NULL,
                                     &saved_err);
                RETURN_THROWS();
            }
            fastjson_set_error_code(FASTJSON_ERROR_UNSUPPORTED_TYPE,
                                    "Encoded JSON string is too large");
            RETURN_FALSE;
        }
        if (splice_status == FJ_SPLICE_INF_OR_NAN) {
            if (throw_mode) {
                fastjson_throw_error(FASTJSON_ERROR_INF_OR_NAN,
                                     "Inf and NaN cannot be JSON encoded", NULL,
                                     &saved_err);
                RETURN_THROWS();
            }
            fastjson_set_error_code(FASTJSON_ERROR_INF_OR_NAN,
                                    "Inf and NaN cannot be JSON encoded");
            RETURN_FALSE;
        }
        if (splice_status == FJ_SPLICE_SETTABLE_FAIL) {
            if (throw_mode) {
                fastjson_throw_error(FASTJSON_ERROR_SYNTAX,
                    "JSON pointer does not resolve to a settable location",
                    NULL, &saved_err);
                RETURN_THROWS();
            }
            fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                "JSON pointer does not resolve to a settable location");
            RETURN_FALSE;
        }
        if (throw_mode) {
            fastjson_throw_error(FASTJSON_ERROR_SYNTAX,
                                 "fastjson_pointer_set write failed", NULL,
                                 &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "fastjson_pointer_set write failed");
        RETURN_FALSE;
    }

    RETVAL_STR(out);
    if (!throw_mode && FASTJSON_G(last_err_code) == FASTJSON_ERROR_NONE) {
        fastjson_clear_error();
    }
}
