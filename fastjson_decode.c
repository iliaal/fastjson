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
            int len = snprintf(buf, sizeof(buf), "%" PRIu64, u);
            ZVAL_STRINGL(out, buf, (size_t)len);
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
            int len = snprintf(buf, sizeof(buf), "%" PRId64, s);
            ZVAL_STRINGL(out, buf, (size_t)len);
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
static bool fastjson_yyval_to_zval(yyjson_val *val, bool assoc,
                                   zend_long remaining_depth,
                                   zend_long flags, zval *out)
{
    /* Depth semantics differ between encode and decode: ext/json's
     * json_decode counts every level (scalar leaves included), while
     * json_encode only counts containers. Cross-check:
     *   json_decode("[[[1]]]", true, 3) -> NULL  (needs depth 4)
     *   json_encode([[[1]]], 0, 3)      -> '[[[1]]]'  (3 containers)
     * Match the decoder's stricter rule by checking depth at the top
     * of every recurse, not just on container entry. */
    if (remaining_depth <= 0) {
        fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                  "Maximum stack depth exceeded");
        ZVAL_NULL(out);
        return false;
    }

    yyjson_type t = yyjson_get_type(val);

    switch (t) {
    case YYJSON_TYPE_NULL:
        ZVAL_NULL(out);
        return true;

    case YYJSON_TYPE_BOOL:
        ZVAL_BOOL(out, yyjson_get_bool(val));
        return true;

    case YYJSON_TYPE_NUM:
        fj_num_to_zval(val, flags, out);
        return true;

    case YYJSON_TYPE_RAW:
        fj_raw_to_zval(val, out);
        return true;

    case YYJSON_TYPE_STR: {
        /* UTF-8 sanitization for JSON_INVALID_UTF8_IGNORE / SUBSTITUTE
         * lives in fastjson_yyval_to_zval_sanitize, picked at
         * fastjson_decode entry. Keeping this walker branch-free
         * preserves the pre-IGNORE/SUBSTITUTE hot-path cost. */
        const char *raw = yyjson_get_str(val);
        size_t raw_len = yyjson_get_len(val);
        /* yyjson_get_len returns the byte length; ZVAL_STRINGL preserves
         * embedded NULs that may appear after a \0 escape. */
        ZVAL_STRINGL(out, raw, raw_len);
        return true;
    }

    case YYJSON_TYPE_ARR: {
        /* Stack-overflow guard for runaway recursion (parallel to the
         * encoder's check; fires when remaining_depth is configured
         * looser than zend.max_allowed_stack_size allows). */
        if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        /* Containers cost a minimum of 2 levels even when empty:
         * ext/json's json_decode("[]", true, 1) returns NULL.
         * The top-of-call check above guards remaining_depth>=1 for
         * any value; here we additionally require >=2 before entering
         * a container. */
        if (remaining_depth <= 1) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = yyjson_arr_size(val);
        array_init_size(out, (uint32_t)n);
        yyjson_val *item;
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(val, &iter);
        while ((item = yyjson_arr_iter_next(&iter))) {
            zval child;
            if (!fastjson_yyval_to_zval(item, assoc,
                                        remaining_depth - 1, flags, &child)) {
                /* `out` is a real zend_array now; partial children get
                 * freed by the caller's zval_ptr_dtor on the failure
                 * path. We do nothing extra here. */
                zval_ptr_dtor(&child);
                return false;
            }
            zend_hash_next_index_insert(Z_ARRVAL_P(out), &child);
        }
        return true;
    }

    case YYJSON_TYPE_OBJ: {
        if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        if (remaining_depth <= 1) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = yyjson_obj_size(val);
        HashTable *ht;
        if (assoc) {
            array_init_size(out, (uint32_t)n);
            ht = Z_ARRVAL_P(out);
        } else {
            object_init(out);
            /* Z_OBJPROP_P triggers lazy properties HashTable creation
             * on the freshly-init'd stdClass. zend_hash_update on it
             * directly avoids the write_property handler dispatch +
             * zval copy that add_property_zval_ex would impose. */
            ht = Z_OBJPROP_P(out);
        }
        yyjson_val *key;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(val, &iter);
        while ((key = yyjson_obj_iter_next(&iter))) {
            yyjson_val *vval = yyjson_obj_iter_get_val(key);
            const char *kstr = yyjson_get_str(key);
            size_t klen = yyjson_get_len(key);
            zval child;
            if (!fastjson_yyval_to_zval(vval, assoc,
                                        remaining_depth - 1, flags, &child)) {
                zval_ptr_dtor(&child);
                return false;
            }
            if (assoc) {
                /* zend_symtable_str_update applies PHP's standard
                 * "0" -> 0 numeric-string-to-int promotion for array
                 * keys. Empty key is allowed and stored as "". */
                zend_symtable_str_update(ht, kstr, klen, &child);
            } else {
                /* Object properties keep string keys verbatim; "0"
                 * stays "0" so $obj->{"0"} works as expected. */
                zend_string *zk = zend_string_init(kstr, klen, 0);
                zend_hash_update(ht, zk, &child);
                zend_string_release_ex(zk, 0);
            }
        }
        return true;
    }

    default:
        /* YYJSON_TYPE_NONE / RAW shouldn't reach a parsed doc with
         * default flags. Fall back to null rather than asserting. */
        ZVAL_NULL(out);
        return true;
    }
}

/* Sanitize-mode walker: same shape as fastjson_yyval_to_zval, but
 * runs fastjson_sanitize_utf8 on string values and object keys
 * (decode-side semantics: SUBSTITUTE wins on BOTH-bits, per-byte
 * advance on invalid). Only invoked when the caller set
 * JSON_INVALID_UTF8_IGNORE / SUBSTITUTE, so the fast walker above
 * pays nothing for the feature. */
static bool fastjson_yyval_to_zval_sanitize(yyjson_val *val, bool assoc,
                                            zend_long remaining_depth,
                                            zend_long flags, zval *out)
{
    if (remaining_depth <= 0) {
        fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                  "Maximum stack depth exceeded");
        ZVAL_NULL(out);
        return false;
    }

    yyjson_type t = yyjson_get_type(val);
    switch (t) {
    case YYJSON_TYPE_NULL:
        ZVAL_NULL(out);
        return true;
    case YYJSON_TYPE_BOOL:
        ZVAL_BOOL(out, yyjson_get_bool(val));
        return true;
    case YYJSON_TYPE_NUM:
        fj_num_to_zval(val, flags, out);
        return true;
    case YYJSON_TYPE_RAW:
        fj_raw_to_zval(val, out);
        return true;
    case YYJSON_TYPE_STR: {
        const char *raw = yyjson_get_str(val);
        size_t raw_len = yyjson_get_len(val);
        /* Fast path: defensive callers often set IGNORE / SUBSTITUTE on
         * every decode even when the payload is clean UTF-8. Scan first
         * so a well-formed string skips the sanitize alloc + copy. */
        if (fastjson_utf8_well_formed(raw, raw_len)) {
            ZVAL_STRINGL(out, raw, raw_len);
            return true;
        }
        size_t sane_len;
        char *sane = fastjson_sanitize_utf8(raw, raw_len, flags,
                                            FJ_SAN_DECODE, &sane_len);
        ZVAL_STRINGL(out, sane, sane_len);
        efree(sane);
        return true;
    }
    case YYJSON_TYPE_ARR: {
        if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        if (remaining_depth <= 1) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = yyjson_arr_size(val);
        array_init_size(out, (uint32_t)n);
        yyjson_val *item;
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(val, &iter);
        while ((item = yyjson_arr_iter_next(&iter))) {
            zval child;
            if (!fastjson_yyval_to_zval_sanitize(item, assoc,
                                                 remaining_depth - 1, flags, &child)) {
                zval_ptr_dtor(&child);
                return false;
            }
            zend_hash_next_index_insert(Z_ARRVAL_P(out), &child);
        }
        return true;
    }
    case YYJSON_TYPE_OBJ: {
        if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        if (remaining_depth <= 1) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = yyjson_obj_size(val);
        HashTable *ht;
        if (assoc) {
            array_init_size(out, (uint32_t)n);
            ht = Z_ARRVAL_P(out);
        } else {
            object_init(out);
            ht = Z_OBJPROP_P(out);
        }
        yyjson_val *key;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(val, &iter);
        while ((key = yyjson_obj_iter_next(&iter))) {
            yyjson_val *vval = yyjson_obj_iter_get_val(key);
            const char *kstr = yyjson_get_str(key);
            size_t klen = yyjson_get_len(key);
            /* Same fast path as the string-value case above: a
             * well-formed key skips the sanitize alloc + copy and uses
             * yyjson's borrowed pointer directly. */
            bool key_owned = false;
            const char *use_key = kstr;
            size_t use_klen = klen;
            if (!fastjson_utf8_well_formed(kstr, klen)) {
                use_key = fastjson_sanitize_utf8(kstr, klen, flags,
                                                 FJ_SAN_DECODE, &use_klen);
                key_owned = true;
            }
            zval child;
            if (!fastjson_yyval_to_zval_sanitize(vval, assoc,
                                                 remaining_depth - 1, flags, &child)) {
                if (key_owned) efree((char *)use_key);
                zval_ptr_dtor(&child);
                return false;
            }
            if (assoc) {
                zend_symtable_str_update(ht, use_key, use_klen, &child);
            } else {
                zend_string *zk = zend_string_init(use_key, use_klen, 0);
                zend_hash_update(ht, zk, &child);
                zend_string_release_ex(zk, 0);
            }
            if (key_owned) efree((char *)use_key);
        }
        return true;
    }
    default:
        ZVAL_NULL(out);
        return true;
    }
}

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
            zend_long code = fastjson_translate_read_code(err.code);
            zend_class_entry *ce = fastjson_json_exception_ce
                ? fastjson_json_exception_ce : zend_ce_exception;
            zend_throw_exception(ce, err.msg, code);
            /* Preserve prior global error state; the exception carries
             * the new error info. */
            fastjson_restore_error_state(saved_err);
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
            zend_class_entry *ce = fastjson_json_exception_ce
                ? fastjson_json_exception_ce : zend_ce_exception;
            zend_throw_exception(ce,
                FASTJSON_G(last_err_msg) ? FASTJSON_G(last_err_msg)
                                         : "Maximum stack depth exceeded",
                FASTJSON_G(last_err_code));
            fastjson_restore_error_state(saved_err);
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
    if (depth <= 0) {
        zend_argument_value_error(3, "must be greater than 0");
        RETURN_THROWS();
    }
    if (depth > INT_MAX) {
        zend_argument_value_error(3, "must be less than %d", INT_MAX);
        RETURN_THROWS();
    }

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
    if (depth <= 0) {
        zend_argument_value_error(3, "must be greater than 0");
        RETURN_THROWS();
    }
    if (depth > INT_MAX) {
        zend_argument_value_error(3, "must be less than %d", INT_MAX);
        RETURN_THROWS();
    }

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    /* Read through the streams layer (wrappers + open_basedir honored).
     * Silent: no REPORT_ERRORS, so a missing/unreadable file emits no
     * warning -- failure is surfaced via the null return + last_error,
     * matching the documented contract. An I/O failure is NOT a JSON
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
        fastjson_set_encode_error(FASTJSON_ERROR_SYNTAX,
                                  "Failed to open file for reading");
        RETURN_NULL();
    }
    zend_string *contents = php_stream_copy_to_mem(stream,
                                                   PHP_STREAM_COPY_ALL, 0);
    /* php_stream_copy_to_mem returns NULL for a zero-byte stream, not an
     * empty zend_string. An empty file is not an I/O failure -- it must
     * decode exactly like fastjson_decode(""): yyjson rejects empty input
     * as a syntax error and JSON_THROW_ON_ERROR throws. Distinguish it
     * from a genuine read error by the stream's EOF state (checked before
     * close). */
    if (contents == NULL && php_stream_eof(stream)) {
        contents = ZSTR_EMPTY_ALLOC();
    }
    php_stream_close(stream);
    if (contents == NULL) {
        fastjson_set_encode_error(FASTJSON_ERROR_SYNTAX,
                                  "Failed to read file");
        RETURN_NULL();
    }

    fastjson_decode_into(ZSTR_VAL(contents), ZSTR_LEN(contents), use_assoc,
                         depth, flags, throw_mode, &saved_err, return_value);
    zend_string_release(contents);
}

/* Throw the JsonException matching a failed read and restore the prior
 * global error state (THROW_ON_ERROR's "exception carries the error,
 * globals untouched" contract). Caller must RETURN_THROWS() after. */
static void fastjson_throw_read_err(const yyjson_read_err *err,
                                    const fastjson_error_state *saved_err)
{
    zend_long code = fastjson_translate_read_code(err->code);
    zend_class_entry *ce = fastjson_json_exception_ce
        ? fastjson_json_exception_ce : zend_ce_exception;
    zend_throw_exception(ce, err->msg, code);
    fastjson_restore_error_state(saved_err);
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
            zend_class_entry *ce = fastjson_json_exception_ce
                ? fastjson_json_exception_ce : zend_ce_exception;
            zend_throw_exception(ce,
                FASTJSON_G(last_err_msg) ? FASTJSON_G(last_err_msg)
                                         : "Maximum stack depth exceeded",
                FASTJSON_G(last_err_code));
            fastjson_restore_error_state(saved_err);
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

    if (depth <= 0) {
        zend_argument_value_error(4, "must be greater than 0");
        RETURN_THROWS();
    }
    if (depth > INT_MAX) {
        zend_argument_value_error(4, "must be less than %d", INT_MAX);
        RETURN_THROWS();
    }

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    yyjson_read_err err;
    yyjson_doc *doc = fastjson_read_doc(json, json_len, flags, &err);
    if (doc == NULL) {
        if (throw_mode) {
            fastjson_throw_read_err(&err, &saved_err);
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

    if (depth <= 0) {
        zend_argument_value_error(4, "must be greater than 0");
        RETURN_THROWS();
    }
    if (depth > INT_MAX) {
        zend_argument_value_error(4, "must be less than %d", INT_MAX);
        RETURN_THROWS();
    }

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    /* Parse both operands as immutable docs first; either failing is a
     * JSON error surfaced through the usual path. */
    yyjson_read_err err;
    yyjson_doc *tdoc = fastjson_read_doc(target, target_len, flags, &err);
    if (tdoc == NULL) {
        if (throw_mode) {
            fastjson_throw_read_err(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(target, target_len, &err);
        RETURN_NULL();
    }
    yyjson_doc *pdoc = fastjson_read_doc(patch, patch_len, flags, &err);
    if (pdoc == NULL) {
        yyjson_doc_free(tdoc);
        if (throw_mode) {
            fastjson_throw_read_err(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(patch, patch_len, &err);
        RETURN_NULL();
    }

    /* The merge and imut-copy below recurse on the C stack per nesting
     * level, ahead of the depth-capped walker. Reject operands whose
     * parsed nesting reaches $depth up front so a pathological input
     * fails with the same FASTJSON_ERROR_DEPTH the walker would raise,
     * rather than overflowing the stack. The merged result's depth never
     * exceeds the deeper operand, so checking both inputs is sufficient. */
    if (fastjson_val_depth_reaches(yyjson_doc_get_root(tdoc), (size_t)depth)
            || fastjson_val_depth_reaches(yyjson_doc_get_root(pdoc),
                                          (size_t)depth)) {
        yyjson_doc_free(tdoc);
        yyjson_doc_free(pdoc);
        if (throw_mode) {
            zend_class_entry *ce = fastjson_json_exception_ce
                ? fastjson_json_exception_ce : zend_ce_exception;
            zend_throw_exception(ce, "Maximum stack depth exceeded",
                                 FASTJSON_ERROR_DEPTH);
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
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
            mdoc, yyjson_doc_get_root(tdoc), yyjson_doc_get_root(pdoc));
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
            zend_class_entry *ce = fastjson_json_exception_ce
                ? fastjson_json_exception_ce : zend_ce_exception;
            zend_throw_exception(ce, "fastjson_merge_patch failed",
                                 FASTJSON_ERROR_SYNTAX);
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_encode_error(FASTJSON_ERROR_SYNTAX,
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
            fastjson_throw_read_err(&err, &saved_err);
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
        Z_PARAM_LONG(flags)
        Z_PARAM_LONG(depth)
    ZEND_PARSE_PARAMETERS_END();

    bool throw_mode = (flags & FASTJSON_DECODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);
    if (!throw_mode) {
        fastjson_clear_error();
    }

    if (depth <= 0) {
        zend_argument_value_error(5, "must be greater than 0");
        RETURN_THROWS();
    }
    if (depth > INT_MAX) {
        zend_argument_value_error(5, "must be less than %d", INT_MAX);
        RETURN_THROWS();
    }

    zend_class_entry *ce = fastjson_json_exception_ce
        ? fastjson_json_exception_ce : zend_ce_exception;

    /* Parse the target document. */
    yyjson_read_err err;
    yyjson_doc *idoc = fastjson_read_doc(json, json_len, flags, &err);
    if (idoc == NULL) {
        if (throw_mode) {
            fastjson_throw_read_err(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_read_error(json, json_len, &err);
        RETURN_FALSE;
    }

    /* yyjson_doc_mut_copy and yyjson_mut_write below recurse on the C stack
     * per nesting level, ahead of any depth cap. Reject a target whose
     * parsed nesting reaches $depth up front (same guard as
     * fastjson_merge_patch) so an adversarial deep input fails cleanly
     * rather than overflowing the stack. */
    if (fastjson_val_depth_reaches(yyjson_doc_get_root(idoc), (size_t)depth)) {
        yyjson_doc_free(idoc);
        if (throw_mode) {
            zend_throw_exception(ce, "Maximum stack depth exceeded",
                                 FASTJSON_ERROR_DEPTH);
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                  "Maximum stack depth exceeded");
        RETURN_FALSE;
    }

    /* Convert $value through the single direct-write encoder, then re-parse
     * it into a node we can splice. Reusing fastjson_directwrite_encode
     * keeps one serialization path for PHP values (matching
     * fastjson_merge_patch's single-encoder rationale) and inherits its
     * recursion and unsupported-type handling. The intermediate encode uses
     * flags=0; output formatting is applied by the final write below. */
    zend_string *vstr = fastjson_directwrite_encode(value, 0, depth);
    if (vstr == NULL) {
        yyjson_doc_free(idoc);
        if (EG(exception)) {
            /* jsonSerialize()/property-hook threw inside the encoder. */
            RETURN_THROWS();
        }
        if (throw_mode) {
            zend_throw_exception(ce,
                FASTJSON_G(last_err_msg) ? FASTJSON_G(last_err_msg)
                                         : "fastjson_pointer_set encode failed",
                FASTJSON_G(last_err_code));
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        /* Encoder already recorded the encode-side error. */
        RETURN_FALSE;
    }

    yyjson_read_err verr;
    yyjson_doc *vdoc = yyjson_read_opts(ZSTR_VAL(vstr), ZSTR_LEN(vstr), 0,
                                        &fastjson_php_alc, &verr);
    zend_string_release(vstr);

    yyjson_mut_doc *mdoc = NULL;
    yyjson_mut_val *mv = NULL;
    if (vdoc != NULL) {
        mdoc = yyjson_doc_mut_copy(idoc, &fastjson_php_alc);
        if (mdoc != NULL) {
            mv = yyjson_val_mut_copy(mdoc, yyjson_doc_get_root(vdoc));
        }
        yyjson_doc_free(vdoc);
    }
    yyjson_doc_free(idoc);

    if (mv == NULL) {
        /* Allocator failure inside yyjson, or the encoder's output failed to
         * re-parse (should not happen -- it emits well-formed JSON). */
        if (mdoc != NULL) {
            yyjson_mut_doc_free(mdoc);
        }
        if (throw_mode) {
            zend_throw_exception(ce, "fastjson_pointer_set failed",
                                 FASTJSON_ERROR_SYNTAX);
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_encode_error(FASTJSON_ERROR_SYNTAX,
                                  "fastjson_pointer_set failed");
        RETURN_FALSE;
    }

    /* The empty pointer "" selects the whole document (RFC 6901); setn
     * cannot replace the root, so swap it directly. Otherwise splice mv in,
     * creating any missing parent objects (yyjson_mut_ptr_setn fails on an
     * array index gap or a scalar mid-path). */
    bool set_ok;
    if (pointer_len == 0) {
        yyjson_mut_doc_set_root(mdoc, mv);
        set_ok = true;
    } else {
        set_ok = yyjson_mut_ptr_setn(yyjson_mut_doc_get_root(mdoc),
                                     pointer, pointer_len, mv, mdoc);
    }
    if (!set_ok) {
        yyjson_mut_doc_free(mdoc);
        if (throw_mode) {
            zend_throw_exception(ce,
                "JSON pointer does not resolve to a settable location",
                FASTJSON_ERROR_SYNTAX);
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_encode_error(FASTJSON_ERROR_SYNTAX,
            "JSON pointer does not resolve to a settable location");
        RETURN_FALSE;
    }

    /* Output formatting from $flags, mirroring dw_translate_yyjson_flags:
     * escape slashes/unicode by default unless the UNESCAPED_* bits are set,
     * and honor PRETTY_PRINT (4-space, matching the direct-write encoder). */
    yyjson_write_flag wflags = 0;
    if (!(flags & FASTJSON_ENCODE_UNESCAPED_SLASHES)) {
        wflags |= YYJSON_WRITE_ESCAPE_SLASHES;
    }
    if (!(flags & FASTJSON_ENCODE_UNESCAPED_UNICODE)) {
        wflags |= YYJSON_WRITE_ESCAPE_UNICODE;
    }
    if (flags & FASTJSON_ENCODE_PRETTY_PRINT) {
        wflags |= YYJSON_WRITE_PRETTY;
    }

    size_t out_len = 0;
    yyjson_write_err werr;
    char *out = yyjson_mut_write_opts(mdoc, wflags, &fastjson_php_alc,
                                      &out_len, &werr);
    yyjson_mut_doc_free(mdoc);

    if (out == NULL) {
        if (throw_mode) {
            zend_throw_exception(ce, "fastjson_pointer_set write failed",
                                 FASTJSON_ERROR_SYNTAX);
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_encode_error(FASTJSON_ERROR_SYNTAX,
                                  "fastjson_pointer_set write failed");
        RETURN_FALSE;
    }

    RETVAL_STRINGL(out, out_len);
    fastjson_php_alc.free(fastjson_php_alc.ctx, out);
    if (!throw_mode) {
        fastjson_clear_error();
    }
}
