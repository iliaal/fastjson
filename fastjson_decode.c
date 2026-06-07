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
#if PHP_VERSION_ID >= 80300
#include "Zend/zend_call_stack.h"
#else
/* zend_call_stack_overflowed() / EG(stack_limit) are 8.3+. On 8.1/8.2
 * the remaining_depth counter (default 512) still bounds recursion, so
 * the secondary C-stack check degrades to a no-op. */
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
        ZVAL_LONG(out, (zend_long)yyjson_get_sint(val));
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

/* Shared decode core for fastjson_decode and fastjson_file_decode.
 * Runs the yyjson read (with the inf/nan-overflow retry and parse-error
 * UTF-8 reclassification), dispatches the appropriate walker into
 * `return_value`, and honors the JSON_THROW_ON_ERROR state-preservation
 * contract. $depth must already be validated and, in non-throw mode,
 * the caller must already have cleared the error state; saved_err_* is
 * the snapshot to restore on the throw path. The param is named
 * `return_value` so RETURN_NULL() / RETURN_THROWS() expand correctly. */
static void fastjson_decode_into(const char *json, size_t json_len,
                                 bool use_assoc, zend_long depth,
                                 zend_long flags, bool throw_mode,
                                 zend_long saved_err_code,
                                 const char *saved_err_msg,
                                 zval *return_value)
{
    /* Translate fastjson decode flags into yyjson_read_flag bits.
     * BIGINT_AS_STRING -> BIGNUM_AS_RAW so numbers that overflow
     * uint64/double are surfaced as YYJSON_TYPE_RAW (raw digit string).
     * The walker handles both that and the uint > INT64_MAX inline
     * stringification. */
    yyjson_read_flag yflags = 0;
    if (flags & FASTJSON_DECODE_BIGINT_AS_STRING) {
        yflags |= YYJSON_READ_BIGNUM_AS_RAW;
    }
    /* IGNORE or SUBSTITUTE: tell yyjson to accept invalid UTF-8 inside
     * JSON strings instead of failing the parse. The sanitize walker
     * (selected below) then strips or replaces those bytes per
     * ext/json's IGNORE/SUBSTITUTE semantics; yyjson itself never
     * leaves raw invalid bytes in the final zval. */
    if (flags & (FASTJSON_DECODE_INVALID_UTF8_IGNORE
                 | FASTJSON_DECODE_INVALID_UTF8_SUBSTITUTE)) {
        yflags |= YYJSON_READ_ALLOW_INVALID_UNICODE;
    }
    /* RELAXED: accept the JSONC subset ext/json rejects (line/block
     * comments, trailing commas, a leading BOM). */
    if (flags & FASTJSON_DECODE_RELAXED) {
        yflags |= YYJSON_READ_ALLOW_COMMENTS
                | YYJSON_READ_ALLOW_TRAILING_COMMAS
                | YYJSON_READ_ALLOW_BOM;
    }

    yyjson_read_err err;
    yyjson_doc *doc = yyjson_read_opts((char *)json, json_len, yflags,
                                       &fastjson_php_alc, &err);
    if (doc == NULL && err.msg
            && strcmp(err.msg, "number is infinity when parsed as double") == 0
            && !fastjson_input_has_inf_nan_literal(json, json_len)) {
        /* yyjson rejects exponent-overflow numbers like "1e309" by
         * default; ext/json accepts and decodes to INF. Retry with
         * ALLOW_INF_AND_NAN to match. The pre-scan keeps inputs that
         * also contain an unquoted Inf/NaN literal token rejected
         * (yyjson's flag would otherwise accept them, but ext/json
         * doesn't). The far more common case (pure overflow, no
         * literal tokens) lines up byte-for-byte. */
        doc = yyjson_read_opts((char *)json, json_len,
                               yflags | YYJSON_READ_ALLOW_INF_AND_NAN,
                               &fastjson_php_alc, &err);
    }
    if (doc == NULL) {
        /* ext/json classifies a MALFORMED UTF-8 byte at a parse-error
         * position as JSON_ERROR_UTF8; a valid-UTF-8-but-not-JSON byte
         * (e.g. bare `é` at top level) stays JSON_ERROR_SYNTAX. yyjson
         * reports both as UNEXPECTED_CHARACTER, so we re-categorize
         * only when the byte sequence is genuinely malformed UTF-8.
         * Matches json_exceptions_error_clearing.phpt. */
        if (err.pos < json_len
                && (unsigned char)json[err.pos] >= 0x80
                && !fastjson_byte_is_valid_utf8_start(json, json_len, err.pos)) {
            err.code = YYJSON_READ_ERROR_INVALID_STRING;
        }
        if (throw_mode) {
            zend_long code = fastjson_translate_read_code(err.code);
            zend_class_entry *ce = fastjson_json_exception_ce
                ? fastjson_json_exception_ce : zend_ce_exception;
            zend_throw_exception(ce, err.msg, code);
            /* Preserve prior global error state; the exception carries
             * the new error info. */
            FASTJSON_G(last_err_code) = saved_err_code;
            FASTJSON_G(last_err_msg) = saved_err_msg;
            RETURN_THROWS();
        }
        fastjson_set_error(err.code, err.msg);
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
            FASTJSON_G(last_err_code) = saved_err_code;
            FASTJSON_G(last_err_msg) = saved_err_msg;
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
    zend_long saved_err_code = FASTJSON_G(last_err_code);
    const char *saved_err_msg = FASTJSON_G(last_err_msg);

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
                         throw_mode, saved_err_code, saved_err_msg,
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
    zend_long saved_err_code = FASTJSON_G(last_err_code);
    const char *saved_err_msg = FASTJSON_G(last_err_msg);

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
                         depth, flags, throw_mode, saved_err_code,
                         saved_err_msg, return_value);
    zend_string_release(contents);
}
