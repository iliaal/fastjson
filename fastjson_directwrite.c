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

/* Write zvals directly to smart_str to avoid per-value yyjson_mut_doc allocation. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "php.h"
#include "Zend/zend_enum.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_smart_str.h"
#include "php_fastjson.h"
#include "fastjson_arginfo.h"
#include "fastjson_alloc.h"
#include "yyjson.h"

typedef struct fastjson_dw_ctx {
    smart_str       buf;
    zend_long       flags;
    yyjson_write_flag yflags;
    /* PG(serialize_precision) sampled once at entry. ext/json formats every
     * double via zend_gcvt(precision); the default -1 means shortest
     * round-trip, which the yyjson fast path below already matches. */
    int             precision;
    bool            partial_output;
    bool            pretty_print;
    bool            hard_error;
    /* Prevent enclosing containers from restarting discard after a hard stop. */
    bool            discard_aborted;
    fastjson_error_state error;
    uint32_t        call_depth;
    int             indent_level;
} fastjson_dw_ctx;

static bool dw_encode_zval(fastjson_dw_ctx *ctx, zval *zv,
                           zend_long remaining_depth);
static bool dw_emit_object_props(fastjson_dw_ctx *ctx, zval *zv,
                                 zend_long remaining_depth);
static bool dw_discard_zval(fastjson_dw_ctx *ctx, zval *zv,
                            zend_long remaining_depth);
static bool dw_discard_object_props(fastjson_dw_ctx *ctx, zval *zv,
                                    zend_long remaining_depth);
static bool dw_discard_array_range(fastjson_dw_ctx *ctx, HashTable *ht,
                                   uint32_t from,
                                   zend_long remaining_depth,
                                   bool as_list);
static bool dw_discard_object_props_range(fastjson_dw_ctx *ctx,
                                          zend_object *obj,
                                          HashTable *props,
                                          uint32_t from,
                                          zend_long remaining_depth);

typedef struct {
#if PHP_VERSION_ID >= 80300
    uint32_t *value;
#else
    HashTable *value;
#endif
} fastjson_dw_json_guard;

static bool dw_json_guard_is_recursive(fastjson_dw_json_guard *guard,
                                       zval *zv)
{
#if PHP_VERSION_ID >= 80300
    guard->value = zend_get_recursion_guard(Z_OBJ_P(zv));
    return ZEND_GUARD_IS_RECURSIVE(guard->value, JSON);
#else
    guard->value = Z_OBJPROP_P(zv);
    return guard->value != NULL && GC_IS_RECURSIVE(guard->value);
#endif
}

static void dw_json_guard_protect(fastjson_dw_json_guard *guard)
{
#if PHP_VERSION_ID >= 80300
    ZEND_GUARD_PROTECT_RECURSION(guard->value, JSON);
#else
    if (guard->value != NULL) GC_TRY_PROTECT_RECURSION(guard->value);
#endif
}

static void dw_json_guard_unprotect(fastjson_dw_json_guard *guard)
{
#if PHP_VERSION_ID >= 80300
    ZEND_GUARD_UNPROTECT_RECURSION(guard->value, JSON);
#else
    if (guard->value != NULL) GC_TRY_UNPROTECT_RECURSION(guard->value);
#endif
}

static void dw_set_error(fastjson_dw_ctx *ctx, zend_long code,
                         const char *msg)
{
    fastjson_error_state_set(&ctx->error, code, msg);
    fastjson_set_error_code(code, msg);
}

static bool dw_fail_too_large(fastjson_dw_ctx *ctx)
{
    dw_set_error(ctx, FASTJSON_ERROR_UNSUPPORTED_TYPE,
                 "Encoded JSON string is too large");
    return false;
}

static bool dw_reserve(fastjson_dw_ctx *ctx, size_t add_len)
{
    size_t cur_len = ctx->buf.s ? ZSTR_LEN(ctx->buf.s) : 0;
    if (UNEXPECTED(add_len > ZSTR_MAX_LEN - cur_len)) {
        return dw_fail_too_large(ctx);
    }
    smart_str_alloc(&ctx->buf, add_len, 0);
    return true;
}

static bool dw_reserve_string(fastjson_dw_ctx *ctx, size_t len)
{
    if (UNEXPECTED(len > (ZSTR_MAX_LEN - 2) / 6)) {
        return dw_fail_too_large(ctx);
    }
    return dw_reserve(ctx, len * 6 + 2);
}

yyjson_write_flag fastjson_translate_write_flags(zend_long php_flags,
                                                 bool with_pretty)
{
    yyjson_write_flag yf = 0;
    if (!(php_flags & FASTJSON_ENCODE_UNESCAPED_SLASHES)) {
        yf |= YYJSON_WRITE_ESCAPE_SLASHES;
    }
    if (!(php_flags & FASTJSON_ENCODE_UNESCAPED_UNICODE)) {
        yf |= YYJSON_WRITE_ESCAPE_UNICODE;
    }
    if (with_pretty && (php_flags & FASTJSON_ENCODE_PRETTY_PRINT)) {
        yf |= YYJSON_WRITE_PRETTY;
    }
    /* Sanitize IGNORE/SUBSTITUTE ourselves; yyjson's invalid-Unicode mode
     * does not match ext/json's strip-or-substitute semantics. */
    return yf;
}

static inline void dw_emit_newline_indent(fastjson_dw_ctx *ctx, int level)
{
    fastjson_append_newline_indent(&ctx->buf, level);
}

static bool dw_apply_hex_escapes(fastjson_dw_ctx *ctx,
                                 size_t start_pos)
{
    if (EXPECTED((ctx->flags & FASTJSON_ENCODE_HEX_MASK) == 0)) {
        return true;
    }
    if (UNEXPECTED(!fastjson_apply_hex_escapes(
            &ctx->buf, ctx->flags, start_pos))) {
        return dw_fail_too_large(ctx);
    }
    return true;
}

/* Partial output substitutes an empty string for invalid keys, null for values. */
static bool dw_emit_string_ex(fastjson_dw_ctx *ctx, const char *s, size_t len,
                              bool is_key)
{
    size_t start_pos = ctx->buf.s ? ZSTR_LEN(ctx->buf.s) : 0;
    if (UNEXPECTED(len >= FASTJSON_EXACT_STRING_THRESHOLD)) {
        fj_string_size_status status = fastjson_write_large_json_string(
            &ctx->buf, s, len, ctx->yflags);
        if (status == FJ_STRING_SIZE_OK) {
            return dw_apply_hex_escapes(ctx, start_pos);
        }
        if (status == FJ_STRING_SIZE_TOO_LARGE) {
            return dw_fail_too_large(ctx);
        }
        goto invalid_utf8;
    }
    if (!dw_reserve_string(ctx, len)) {
        return false;
    }
    char *cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
    char *end = yyjson_write_string_to_buf(cur, s, len, ctx->yflags);
    if (EXPECTED(end != NULL)) {
        ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
        return dw_apply_hex_escapes(ctx, start_pos);
    }

invalid_utf8:
    {
        /* Allocate a sanitized copy only after the writer rejects the input. */
        if (FASTJSON_HAS_UTF8_HANDLING_FLAG(ctx->flags)) {
            size_t sane_len;
            char *sane = fastjson_sanitize_utf8(s, len, ctx->flags,
                                                FJ_SAN_ENCODE, &sane_len);
            fj_string_size_status sane_status = FJ_STRING_SIZE_OK;
            if (sane_len >= FASTJSON_EXACT_STRING_THRESHOLD) {
                sane_status = fastjson_write_large_json_string(
                    &ctx->buf, sane, sane_len, ctx->yflags);
                end = sane_status == FJ_STRING_SIZE_OK
                    ? ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s) : NULL;
            } else if (dw_reserve_string(ctx, sane_len)) {
                cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
                end = yyjson_write_string_to_buf(
                    cur, sane, sane_len, ctx->yflags);
            } else {
                sane_status = FJ_STRING_SIZE_TOO_LARGE;
                end = NULL;
            }
            efree(sane);
            if (EXPECTED(end != NULL)) {
                if (sane_len < FASTJSON_EXACT_STRING_THRESHOLD) {
                    ZSTR_LEN(ctx->buf.s) = (size_t)(end
                        - ZSTR_VAL(ctx->buf.s));
                }
                return dw_apply_hex_escapes(ctx, start_pos);
            }
            if (sane_status == FJ_STRING_SIZE_TOO_LARGE) {
                return dw_fail_too_large(ctx);
            }
        }
        dw_set_error(ctx, FASTJSON_ERROR_UTF8,
            "Malformed UTF-8 characters, possibly incorrectly encoded");
        if (ctx->partial_output) {
            if (is_key) {
                /* ext/json substitutes an empty quoted key here. */
                smart_str_appendl(&ctx->buf, "\"\"", 2);
            } else {
                smart_str_appendl(&ctx->buf, "null", 4);
            }
            return true;
        }
        return false;
    }
}

static inline bool dw_emit_string(fastjson_dw_ctx *ctx, const char *s, size_t len)
{
    return dw_emit_string_ex(ctx, s, len, false);
}

static void dw_emit_long(fastjson_dw_ctx *ctx, zend_long n)
{
    smart_str_alloc(&ctx->buf, FASTJSON_NUM_INT_WORST, 0);
    yyjson_val v;
    v.tag = (uint64_t)(YYJSON_TYPE_NUM | YYJSON_SUBTYPE_SINT);
    v.uni.i64 = (int64_t)n;
    char *cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
    char *end = yyjson_write_number(&v, cur);
    ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
}

static bool dw_emit_double(fastjson_dw_ctx *ctx, double d)
{
    if (!isfinite(d)) {
        dw_set_error(ctx, FASTJSON_ERROR_INF_OR_NAN,
            "Inf and NaN cannot be JSON encoded");
        if (ctx->partial_output) {
            /* ext/json's substitution for INF/NaN is JSON `0` per
             * inf_nan_error.phpt -- not null. */
            smart_str_appendc(&ctx->buf, '0');
            return true;
        }
        ctx->hard_error = true;
        smart_str_free(&ctx->buf);
        return false;
    }
    /* Negative zero: ext/json emits "-0" (without PRESERVE_ZERO_FRACTION)
     * or "-0.0" (with). The long-path shortcut would cast (zend_long)-0.0
     * to 0 and lose the sign; yyjson's writer always emits "-0.0" with
     * the trailing fraction. Emit the right literal directly. */
    if (d == 0.0 && signbit(d)) {
        if (ctx->flags & FASTJSON_ENCODE_PRESERVE_ZERO_FRACTION) {
            smart_str_appendl(&ctx->buf, "-0.0", 4);
        } else {
            smart_str_appendl(&ctx->buf, "-0", 2);
        }
        return true;
    }
    /* Non-default serialize_precision: ext/json formats every double with
     * zend_gcvt(precision), so the long shortcut and yyjson shortest
     * round-trip below would both diverge. Mirror php_json_encode_double. */
    if (UNEXPECTED(ctx->precision != -1)) {
        char num[ZEND_DOUBLE_MAX_LENGTH];
        php_gcvt(d, ctx->precision, '.', 'e', num);
        size_t len = strlen(num);
        if ((ctx->flags & FASTJSON_ENCODE_PRESERVE_ZERO_FRACTION)
                && strchr(num, '.') == NULL
                && len < ZEND_DOUBLE_MAX_LENGTH - 2) {
            num[len++] = '.';
            num[len++] = '0';
            num[len] = '\0';
        }
        smart_str_appendl(&ctx->buf, num, len);
        return true;
    }

    /* PRESERVE_ZERO_FRACTION polarity: ext/json's default emits
     * integer-valued doubles WITHOUT the .0 ("1230.0" -> "1230");
     * the flag asks to keep .0 (yyjson's default). When the flag is
     * unset and the value is integer-valued + lossless, route through
     * the long path. The bound stays strictly inside zend_long so the
     * cast is defined and lines up with the range where json_encode
     * itself prefers fixed-notation over scientific.
     *
     * 64-bit: strict `< 1e17`. php_gcvt at serialize_precision=-1 emits
     * fixed-notation for integer-valued doubles up to but not including
     * 1e17 (where it switches to "1.0e+17"). Matching that cutoff means
     * 1e16, 1.5e16, 2.5e16, 9.99e16 all round-trip byte-identically to
     * json_encode. Above 1e17 we fall through to yyjson's REAL writer
     * and accept the residual ".0" divergence -- agreeing with
     * json_encode there would need a scientific-notation writer.
     * 1e17 fits in int64_t with room to spare (ZEND_LONG_MAX ~ 9.22e18)
     * and is well below 2^63, so the cast stays defined; the widely-used
     * `<= (double)ZEND_LONG_MAX` idiom is unsafe at the boundary because
     * that conversion rounds to 2^63.
     *
     * 32-bit (zend_long = int32_t, ZEND_LONG_MAX = 2^31 - 1): 2147483647
     * fits in a double exactly, so the canonical `<= (double)ZEND_LONG_MAX`
     * idiom is safe and used as-is.
     *
     * Order: cheap flag + bound test first (a single fabs + compare),
     * then cast and verify integer-ness via (double)l == d. Avoids the
     * libm floor() call on the common non-integer / out-of-range path
     * (number-heavy double arrays previously paid floor() per element). */
    if (!(ctx->flags & FASTJSON_ENCODE_PRESERVE_ZERO_FRACTION)
#if SIZEOF_ZEND_LONG >= 8
            && fabs(d) < 1e17
#else
            && fabs(d) <= (double)ZEND_LONG_MAX
#endif
        ) {
        zend_long l = (zend_long)d;
        if ((double)l == d) {
            dw_emit_long(ctx, l);
            return true;
        }
    }
    smart_str_alloc(&ctx->buf, FASTJSON_NUM_REAL_WORST, 0);
    yyjson_val v;
    v.tag = (uint64_t)(YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL);
    v.uni.f64 = d;
    char *cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
    char *end = yyjson_write_number(&v, cur);
    if (UNEXPECTED(end == NULL)) {
        /* Should be unreachable since we filtered !isfinite above. */
        dw_set_error(ctx, FASTJSON_ERROR_INF_OR_NAN,
            "Inf and NaN cannot be JSON encoded");
        if (ctx->partial_output) {
            smart_str_appendc(&ctx->buf, '0');
            return true;
        }
        return false;
    }
    ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
    return true;
}

static bool dw_partial_or_fail(fastjson_dw_ctx *ctx,
                               int error_code,
                               const char *error_msg,
                               bool emit_zero_not_null)
{
    dw_set_error(ctx, error_code, error_msg);
    if (ctx->partial_output) {
        if (emit_zero_not_null) {
            smart_str_appendc(&ctx->buf, '0');
        } else {
            smart_str_appendl(&ctx->buf, "null", 4);
        }
        return true;
    }
    return false;
}

/* Emit an object key followed by ':'. Object keys are always strings
 * in JSON; for integer keys we stringify the long. NUMERIC_CHECK
 * doesn't apply to keys (ext/json only honors it for values).
 * Returns false if the caller should abort the encode (non-partial
 * UTF-8 failure on a string key). */
static bool dw_emit_object_key(fastjson_dw_ctx *ctx, zend_string *key,
                               zend_ulong index)
{
    if (key) {
        if (!dw_emit_string_ex(ctx, ZSTR_VAL(key), ZSTR_LEN(key), true)) {
            return false;
        }
    } else {
        /* Integer keys need no escaping; reuse yyjson's digit writer. */
        if (!dw_reserve(ctx, FASTJSON_NUM_INT_WORST + 2)) {
            return false;
        }
        smart_str_appendc(&ctx->buf, '"');
        yyjson_val v;
        v.tag = (uint64_t)(YYJSON_TYPE_NUM | YYJSON_SUBTYPE_SINT);
        v.uni.i64 = (int64_t)(zend_long)index;
        char *cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
        char *end = yyjson_write_number(&v, cur);
        ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
        smart_str_appendc(&ctx->buf, '"');
    }
    smart_str_appendc(&ctx->buf, ':');
    if (ctx->pretty_print) {
        smart_str_appendc(&ctx->buf, ' ');
    }
    return true;
}

/* Keep the error-only discard traversal out of the common encoder's
 * register allocation; inlining this function measurably slowed scalar arrays. */
static zend_never_inline bool dw_emit_array(fastjson_dw_ctx *ctx, HashTable *ht,
                                            zend_long remaining_depth,
                                            bool force_object)
{
    if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }
    /* remaining_depth > INT_MAX means the caller passed a $depth above
     * INT_MAX. ext/json stores max_depth in an int, so such a depth wraps
     * non-positive and every container trips JSON_ERROR_DEPTH; match that
     * (and keep the C-stack cap meaningful where zend_call_stack_overflowed
     * is unavailable). Scalars never reach here, so a huge $depth on a
     * scalar still encodes cleanly, as in ext/json. */
    if (remaining_depth <= 0 || remaining_depth > INT_MAX) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }

    /* Empty arrays are typically the GC_IMMUTABLE singleton; don't
     * flip recursion bits on read-only memory. Immutables are by
     * definition not cyclic. */
    bool need_recursion_guard = !(GC_FLAGS(ht) & GC_IMMUTABLE);
    if (need_recursion_guard) {
        if (GC_IS_RECURSIVE(ht)) {
            return dw_partial_or_fail(ctx, FASTJSON_ERROR_RECURSION,
                "Recursion detected", false);
        }
        GC_PROTECT_RECURSION(ht);
    }

    bool as_list = !force_object && zend_array_is_list(ht);
    bool pretty = ctx->pretty_print;
    bool empty = zend_array_count(ht) == 0;

    smart_str_appendc(&ctx->buf, as_list ? '[' : '{');
    if (pretty && !empty) ctx->indent_level++;

    bool first = true;
    if (as_list) {
        zval *item;
        ZEND_HASH_FOREACH_VAL(ht, item) {
            ZVAL_DEREF(item);
            if (!first) smart_str_appendc(&ctx->buf, ',');
            if (pretty) dw_emit_newline_indent(ctx, ctx->indent_level);
            if (UNEXPECTED(!dw_encode_zval(
                    ctx, item, remaining_depth - 1))) {
                if (ctx->hard_error && !ctx->discard_aborted) {
                    /* Resume at the element after the one that failed.
                     * ZEND_HASH_FOREACH_VAL expands to
                     * _ZEND_HASH_FOREACH_VAL, which declares only `_z` (the
                     * current element) and advances it in the loop's
                     * increment -- hence the +1. The key/value loops use
                     * ZEND_HASH_FOREACH_FROM instead, where `__z` is already
                     * advanced and is not in scope here. The two spellings
                     * are not interchangeable. */
#if PHP_VERSION_ID < 80200
                    uint32_t from = (uint32_t)((_p + 1) - __ht->arData);
#else
                    uint32_t from = HT_IS_PACKED(__ht)
                        ? (uint32_t)((_z + 1) - __ht->arPacked)
                        : (uint32_t)(((Bucket *)_z + 1) - __ht->arData);
#endif
                    if (!dw_discard_array_range(ctx, ht, from,
                            remaining_depth - 1, true)) {
                        ctx->discard_aborted = true;
                    }
                }
                if (need_recursion_guard) GC_UNPROTECT_RECURSION(ht);
                if (pretty && !empty) ctx->indent_level--;
                return false;
            }
            first = false;
        } ZEND_HASH_FOREACH_END();
    } else {
        zend_string *key;
        zend_ulong index;
        zval *item;
        ZEND_HASH_FOREACH_KEY_VAL(ht, index, key, item) {
            ZVAL_DEREF(item);
            if (!first) smart_str_appendc(&ctx->buf, ',');
            if (pretty) dw_emit_newline_indent(ctx, ctx->indent_level);
            if (UNEXPECTED(!dw_emit_object_key(ctx, key, index)
                    || !dw_encode_zval(ctx, item, remaining_depth - 1))) {
                if (ctx->hard_error && !ctx->discard_aborted) {
                    /* `__z` is pre-advanced by ZEND_HASH_FOREACH_FROM, so it
                     * already names the next element; no +1 here. */
#if PHP_VERSION_ID < 80200
                    uint32_t from = (uint32_t)((_p + 1) - __ht->arData);
#else
                    uint32_t from = HT_IS_PACKED(__ht)
                        ? (uint32_t)(__z - __ht->arPacked)
                        : (uint32_t)((Bucket *)__z - __ht->arData);
#endif
                    if (!dw_discard_array_range(ctx, ht, from,
                            remaining_depth - 1, false)) {
                        ctx->discard_aborted = true;
                    }
                }
                if (need_recursion_guard) GC_UNPROTECT_RECURSION(ht);
                if (pretty && !empty) ctx->indent_level--;
                return false;
            }
            first = false;
        } ZEND_HASH_FOREACH_END();
    }
    if (pretty && !empty) {
        ctx->indent_level--;
        dw_emit_newline_indent(ctx, ctx->indent_level);
    }
    smart_str_appendc(&ctx->buf, as_list ? ']' : '}');

    if (need_recursion_guard) GC_UNPROTECT_RECURSION(ht);
    return true;
}

static bool dw_emit_jsonserializable(fastjson_dw_ctx *ctx, zval *zv,
                                     zend_long remaining_depth)
{
    /* Capture the stable object pointer before entering userland. */
    zend_object *obj = Z_OBJ_P(zv);
    fastjson_dw_json_guard guard;
    if (dw_json_guard_is_recursive(&guard, zv)) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_RECURSION,
            "Recursion detected", false);
    }
    /* jsonSerialize() can drop every other reference to obj (bug77843). */
    GC_ADDREF(obj);
    dw_json_guard_protect(&guard);

    zval retval;
    ZVAL_UNDEF(&retval);
    zend_call_method_with_0_params(obj, obj->ce, NULL,
                                   "jsonserialize", &retval);
    if (EG(exception)) {
        dw_json_guard_unprotect(&guard);
        zval_ptr_dtor(&retval);
        OBJ_RELEASE(obj);
        return false;
    }

    /* jsonSerialize() adds no container level; its result keeps the object's depth.
     * A $this result emits properties without re-entering jsonSerialize();
     * the property walker supplies its own recursion guard. */
    if (Z_TYPE(retval) == IS_OBJECT && Z_OBJ(retval) == obj) {
        dw_json_guard_unprotect(&guard);
        bool ok;
        if (remaining_depth <= 0 || remaining_depth > INT_MAX) {
            ok = dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
                "Maximum stack depth exceeded", false);
        } else {
            ok = dw_emit_object_props(ctx, &retval, remaining_depth);
        }
        zval_ptr_dtor(&retval);
        OBJ_RELEASE(obj);
        return ok;
    }

    bool ok = dw_encode_zval(ctx, &retval, remaining_depth);
    dw_json_guard_unprotect(&guard);
    zval_ptr_dtor(&retval);
    OBJ_RELEASE(obj);
    return ok;
}

static bool dw_emit_object(fastjson_dw_ctx *ctx, zval *zv,
                           zend_long remaining_depth)
{
    if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }
    if (fastjson_json_serializable_ce != NULL
            && instanceof_function(Z_OBJCE_P(zv),
                                   fastjson_json_serializable_ce)) {
        return dw_emit_jsonserializable(ctx, zv, remaining_depth);
    }

    /* JsonSerializable takes precedence even for enums, matching ext/json. */
    if (Z_OBJCE_P(zv)->ce_flags & ZEND_ACC_ENUM) {
        if (Z_OBJCE_P(zv)->enum_backing_type == IS_UNDEF) {
            return dw_partial_or_fail(ctx, FASTJSON_ERROR_NON_BACKED_ENUM,
                "Non-backed enums have no default serialization", true);
        }
        zval *backing = zend_enum_fetch_case_value(Z_OBJ_P(zv));
        return dw_encode_zval(ctx, backing, remaining_depth);
    }

    /* See dw_emit_array: a $depth above INT_MAX wraps non-positive in
     * ext/json and trips JSON_ERROR_DEPTH on every container; match it.
     * JsonSerializable and backed enums dispatch first because scalar
     * results do not add a container level. */
    if (remaining_depth <= 0 || remaining_depth > INT_MAX) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }

    return dw_emit_object_props(ctx, zv, remaining_depth);
}

/* Encode an object's JSON property view as a `{...}` body, skipping the
 * JsonSerializable / enum dispatch dw_emit_object does up front. Called
 * directly for the jsonSerialize()-returns-$this case, which must emit
 * properties rather than re-invoke jsonSerialize(). */
static bool dw_emit_object_props(fastjson_dw_ctx *ctx, zval *zv,
                                 zend_long remaining_depth)
{
    zend_object *obj = Z_OBJ_P(zv);
    /* Use the JSON-purpose property view so engine objects (DateTime,
     * ArrayObject, etc.) get the same property set ext/json sees, and
     * stdClass's protected/private members get filtered out at the
     * source. Must be paired with zend_release_properties() on every
     * exit path. */
    HashTable *props = zend_get_properties_for(zv, ZEND_PROP_PURPOSE_JSON);
    if (props == NULL) {
        if (EG(exception)) {
            return false;
        }
        smart_str_appendl(&ctx->buf, "{}", 2);
        return true;
    }

    zend_refcounted *recursion_rc = (zend_refcounted *)props;
#if PHP_VERSION_ID >= 80400
    if (obj->ce->num_hooked_props != 0) {
        recursion_rc = (zend_refcounted *)obj;
    }
#endif
    bool need_recursion_guard = !(GC_FLAGS(recursion_rc) & GC_IMMUTABLE);
    if (need_recursion_guard) {
        if (GC_IS_RECURSIVE(recursion_rc)) {
            zend_release_properties(props);
            return dw_partial_or_fail(ctx, FASTJSON_ERROR_RECURSION,
                "Recursion detected", false);
        }
        GC_PROTECT_RECURSION(recursion_rc);
    }

    bool pretty = ctx->pretty_print;
    /* Hidden, uninitialized, and hookless virtual properties count but emit
     * nothing. Delay pretty indentation until the first emitted property. */
    smart_str_appendc(&ctx->buf, '{');

    bool first = true;
    bool body_open = false;
    zend_string *key;
    zend_ulong index;
    zval *item;
    ZEND_HASH_FOREACH_KEY_VAL(props, index, key, item) {
        /* Custom property handlers may return mangled private/protected keys. */
        if (key && ZSTR_VAL(key)[0] == '\0' && ZSTR_LEN(key) > 0) {
            continue;
        }
        /* Declared properties use indirect zvals into the object's property table. */
        if (Z_TYPE_P(item) == IS_INDIRECT) {
            item = Z_INDIRECT_P(item);
        }
        /* PHP 8.4 represents hooks as IS_PTR property metadata; retain hook_rv
         * until its value has been encoded. */
        zval hook_rv;
        bool release_hook_rv = false;
        if (Z_TYPE_P(item) == IS_PTR) {
            zend_property_info *info = Z_PTR_P(item);
#if PHP_VERSION_ID >= 80400
            /* Virtual properties without a get hook contribute no value
             * to the JSON output (ext/json skips them; otherwise the
             * read_property would call into hookless storage that
             * returns undef or throws). */
            if ((info->flags & ZEND_ACC_VIRTUAL)
                    && (!info->hooks || !info->hooks[ZEND_PROPERTY_HOOK_GET])) {
                continue;
            }
#endif
            ZVAL_UNDEF(&hook_rv);
            zval *hooked = zend_read_property_ex(info->ce, obj,
                                                 info->name, true, &hook_rv);
            if (EG(exception)) {
                /* A throwing hook may still own a refcounted hook_rv. */
                zval_ptr_dtor(&hook_rv);
                if (need_recursion_guard) GC_UNPROTECT_RECURSION(recursion_rc);
                if (body_open) ctx->indent_level--;
                zend_release_properties(props);
                return false;
            }
            item = hooked;
            release_hook_rv = true;
        }
        if (Z_ISUNDEF_P(item)) {
            if (release_hook_rv) zval_ptr_dtor(&hook_rv);
            continue;
        }
        ZVAL_DEREF(item);
        if (pretty && !body_open) { ctx->indent_level++; body_open = true; }
        if (!first) smart_str_appendc(&ctx->buf, ',');
        if (pretty) dw_emit_newline_indent(ctx, ctx->indent_level);
        if (UNEXPECTED(!dw_emit_object_key(ctx, key, index)
                || !dw_encode_zval(ctx, item, remaining_depth - 1))) {
            if (ctx->hard_error && !ctx->discard_aborted) {
                /* `__z` is pre-advanced by ZEND_HASH_FOREACH_FROM, so it
                 * already names the next element; no +1 here. */
#if PHP_VERSION_ID < 80200
                uint32_t from = (uint32_t)((_p + 1) - __ht->arData);
#else
                uint32_t from = (uint32_t)((Bucket *)__z
                    - __ht->arData);
#endif
                if (release_hook_rv) {
                    zval_ptr_dtor(&hook_rv);
                    release_hook_rv = false;
                }
                if (!dw_discard_object_props_range(ctx, obj, props, from,
                        remaining_depth - 1)) {
                    ctx->discard_aborted = true;
                }
            }
            if (release_hook_rv) zval_ptr_dtor(&hook_rv);
            if (need_recursion_guard) GC_UNPROTECT_RECURSION(recursion_rc);
            if (body_open) ctx->indent_level--;
            zend_release_properties(props);
            return false;
        }
        if (release_hook_rv) zval_ptr_dtor(&hook_rv);
        first = false;
    } ZEND_HASH_FOREACH_END();
    if (body_open) {
        ctx->indent_level--;
        dw_emit_newline_indent(ctx, ctx->indent_level);
    }
    smart_str_appendc(&ctx->buf, '}');

    if (need_recursion_guard) GC_UNPROTECT_RECURSION(recursion_rc);
    zend_release_properties(props);
    return true;
}

static bool dw_discard_string(fastjson_dw_ctx *ctx,
                              const char *s, size_t len)
{
    if (UNEXPECTED(len > (ZSTR_MAX_LEN - 2) / 6)) {
        return dw_fail_too_large(ctx);
    }
    if (FASTJSON_HAS_UTF8_HANDLING_FLAG(ctx->flags)
            || fastjson_utf8_well_formed(s, len)) {
        return true;
    }
    dw_set_error(ctx, FASTJSON_ERROR_UTF8,
        "Malformed UTF-8 characters, possibly incorrectly encoded");
    return false;
}

static bool dw_discard_array_range(fastjson_dw_ctx *ctx, HashTable *ht,
                                   uint32_t from,
                                   zend_long remaining_depth,
                                   bool as_list)
{
    if (from >= ht->nNumUsed) {
        return true;
    }

    if (as_list) {
        ZEND_HASH_FOREACH_FROM(ht, 0, from) {
            zval *item = _z;
            ZVAL_DEREF(item);
            if (!dw_discard_zval(ctx, item, remaining_depth)) {
                return false;
            }
        } ZEND_HASH_FOREACH_END();
    } else {
        zend_string *key;
        zval *item;
        ZEND_HASH_FOREACH_STR_KEY_VAL_FROM(ht, key, item, from) {
            if (key && !dw_discard_string(
                    ctx, ZSTR_VAL(key), ZSTR_LEN(key))) {
                return false;
            }
            ZVAL_DEREF(item);
            if (!dw_discard_zval(ctx, item, remaining_depth)) {
                return false;
            }
        } ZEND_HASH_FOREACH_END();
    }
    return true;
}

static bool dw_discard_array(fastjson_dw_ctx *ctx, HashTable *ht,
                             zend_long remaining_depth, bool force_object)
{
    if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }
    if (remaining_depth <= 0 || remaining_depth > INT_MAX) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }

    bool need_recursion_guard = !(GC_FLAGS(ht) & GC_IMMUTABLE);
    if (need_recursion_guard) {
        if (GC_IS_RECURSIVE(ht)) {
            return dw_partial_or_fail(ctx, FASTJSON_ERROR_RECURSION,
                "Recursion detected", false);
        }
        GC_PROTECT_RECURSION(ht);
    }

    bool as_list = !force_object && zend_array_is_list(ht);
    bool ok = dw_discard_array_range(
        ctx, ht, 0, remaining_depth - 1, as_list);
    if (need_recursion_guard) GC_UNPROTECT_RECURSION(ht);
    return ok;
}

static bool dw_discard_object_property(fastjson_dw_ctx *ctx,
                                       zend_object *obj,
                                       zend_string *key,
                                       zval *item,
                                       zend_long remaining_depth)
{
    (void)obj;
    if (key && ZSTR_LEN(key) > 0 && ZSTR_VAL(key)[0] == '\0') {
        return true;
    }
    if (Z_TYPE_P(item) == IS_INDIRECT) {
        item = Z_INDIRECT_P(item);
    }

    /* Re-entering getters could repeat side effects or mask the hard error. */
    if (Z_TYPE_P(item) == IS_PTR) {
        return true;
    }

    if (Z_ISUNDEF_P(item)) {
        return true;
    }
    if (key && !dw_discard_string(ctx, ZSTR_VAL(key), ZSTR_LEN(key))) {
        return false;
    }

    ZVAL_DEREF(item);
    return dw_discard_zval(ctx, item, remaining_depth);
}

static bool dw_discard_object_props_range(fastjson_dw_ctx *ctx,
                                          zend_object *obj,
                                          HashTable *props,
                                          uint32_t from,
                                          zend_long remaining_depth)
{
    if (from >= props->nNumUsed) {
        return true;
    }
    zend_string *key;
    zval *item;
    ZEND_HASH_FOREACH_STR_KEY_VAL_FROM(props, key, item, from) {
        if (!dw_discard_object_property(
                ctx, obj, key, item, remaining_depth)) {
            return false;
        }
    } ZEND_HASH_FOREACH_END();
    return true;
}

static bool dw_discard_object_props(fastjson_dw_ctx *ctx, zval *zv,
                                    zend_long remaining_depth)
{
    zend_object *obj = Z_OBJ_P(zv);
    HashTable *props = zend_get_properties_for(zv, ZEND_PROP_PURPOSE_JSON);
    if (props == NULL) {
        return !EG(exception);
    }

    zend_refcounted *recursion_rc = (zend_refcounted *)props;
#if PHP_VERSION_ID >= 80400
    if (obj->ce->num_hooked_props != 0) {
        recursion_rc = (zend_refcounted *)obj;
    }
#endif
    bool need_recursion_guard = !(GC_FLAGS(recursion_rc) & GC_IMMUTABLE);
    if (need_recursion_guard) {
        if (GC_IS_RECURSIVE(recursion_rc)) {
            zend_release_properties(props);
            return dw_partial_or_fail(ctx, FASTJSON_ERROR_RECURSION,
                "Recursion detected", false);
        }
        GC_PROTECT_RECURSION(recursion_rc);
    }

    bool ok = dw_discard_object_props_range(
        ctx, obj, props, 0, remaining_depth - 1);
    if (need_recursion_guard) GC_UNPROTECT_RECURSION(recursion_rc);
    zend_release_properties(props);
    return ok;
}

static bool dw_discard_jsonserializable(fastjson_dw_ctx *ctx, zval *zv,
                                        zend_long remaining_depth)
{
    /* Do not re-enter serializers during discard: side effects or exceptions
     * could replace the recorded error. */
    (void)ctx;
    (void)zv;
    (void)remaining_depth;
    return true;
}

static bool dw_discard_object(fastjson_dw_ctx *ctx, zval *zv,
                              zend_long remaining_depth)
{
    if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }
    if (fastjson_json_serializable_ce != NULL
            && instanceof_function(Z_OBJCE_P(zv),
                                   fastjson_json_serializable_ce)) {
        return dw_discard_jsonserializable(ctx, zv, remaining_depth);
    }
    if (Z_OBJCE_P(zv)->ce_flags & ZEND_ACC_ENUM) {
        if (Z_OBJCE_P(zv)->enum_backing_type == IS_UNDEF) {
            return dw_partial_or_fail(ctx, FASTJSON_ERROR_NON_BACKED_ENUM,
                "Non-backed enums have no default serialization", true);
        }
        zval *backing = zend_enum_fetch_case_value(Z_OBJ_P(zv));
        return dw_discard_zval(ctx, backing, remaining_depth);
    }
    if (remaining_depth <= 0 || remaining_depth > INT_MAX) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }
    return dw_discard_object_props(ctx, zv, remaining_depth);
}

static bool dw_discard_zval_inner(fastjson_dw_ctx *ctx, zval *zv,
                                  zend_long remaining_depth)
{
    switch (Z_TYPE_P(zv)) {
    case IS_NULL:
    case IS_TRUE:
    case IS_FALSE:
    case IS_LONG:
        return true;
    case IS_DOUBLE:
        /* Discard-found INF must not overwrite the recorded hard error. */
        if (!isfinite(Z_DVAL_P(zv)) && !ctx->hard_error) {
            dw_set_error(ctx, FASTJSON_ERROR_INF_OR_NAN,
                "Inf and NaN cannot be JSON encoded");
        }
        return true;
    case IS_STRING: {
        if (ctx->flags & FASTJSON_ENCODE_NUMERIC_CHECK) {
            const char *s = Z_STRVAL_P(zv);
            size_t slen = Z_STRLEN_P(zv);
            if (slen > 0) {
                unsigned char c0 = (unsigned char)s[0];
                if (!isspace(c0) && c0 != '-' && c0 != '+'
                        && c0 != '.' && !isdigit(c0)) {
                    goto discard_string;
                }
            }
            zend_long lval;
            double dval;
            uint8_t t = is_numeric_string(s, slen, &lval, &dval, 0);
            if (t == IS_LONG || (t == IS_DOUBLE && isfinite(dval))) {
                return true;
            }
        }
discard_string:
        return dw_discard_string(ctx, Z_STRVAL_P(zv), Z_STRLEN_P(zv));
    }
    case IS_ARRAY: {
        bool force_object = (ctx->flags & FASTJSON_ENCODE_FORCE_OBJECT) != 0;
        zval arr_copy;
        ZVAL_COPY(&arr_copy, zv);
        bool ok = dw_discard_array(ctx, Z_ARRVAL(arr_copy), remaining_depth,
                                   force_object);
        zval_ptr_dtor_nogc(&arr_copy);
        return ok;
    }
    case IS_OBJECT:
        return dw_discard_object(ctx, zv, remaining_depth);
    case IS_REFERENCE:
        ZVAL_DEREF(zv);
        return dw_discard_zval(ctx, zv, remaining_depth);
    default:
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_UNSUPPORTED_TYPE,
            "Type is not supported", false);
    }
}

static bool dw_discard_zval(fastjson_dw_ctx *ctx, zval *zv,
                            zend_long remaining_depth)
{
#if !FASTJSON_HAVE_NATIVE_STACK_LIMIT
    if (ctx->call_depth >= 1024) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }
    ctx->call_depth++;
    bool ok = dw_discard_zval_inner(ctx, zv, remaining_depth);
    ctx->call_depth--;
    return ok;
#else
    return dw_discard_zval_inner(ctx, zv, remaining_depth);
#endif
}

static bool dw_encode_zval_inner(fastjson_dw_ctx *ctx, zval *zv,
                                 zend_long remaining_depth)
{
    switch (Z_TYPE_P(zv)) {
    case IS_NULL:
        smart_str_appendl(&ctx->buf, "null", 4);
        return true;
    case IS_TRUE:
        smart_str_appendl(&ctx->buf, "true", 4);
        return true;
    case IS_FALSE:
        smart_str_appendl(&ctx->buf, "false", 5);
        return true;
    case IS_LONG:
        dw_emit_long(ctx, Z_LVAL_P(zv));
        return true;
    case IS_DOUBLE:
        return dw_emit_double(ctx, Z_DVAL_P(zv));
    case IS_STRING: {
        /* Overflow-to-INF remains a string under NUMERIC_CHECK (bug64695). */
        if (ctx->flags & FASTJSON_ENCODE_NUMERIC_CHECK) {
            const char *s = Z_STRVAL_P(zv);
            size_t slen = Z_STRLEN_P(zv);
            if (slen > 0) {
                unsigned char c0 = (unsigned char)s[0];
                /* is_numeric_string() accepts leading whitespace. */
                if (!isspace(c0) && c0 != '-' && c0 != '+'
                        && c0 != '.' && !isdigit(c0)) {
                    goto emit_string;
                }
            }
            zend_long lval;
            double dval;
            uint8_t t = is_numeric_string(s, slen, &lval, &dval, 0);
            if (t == IS_LONG) {
                dw_emit_long(ctx, lval);
                return true;
            }
            if (t == IS_DOUBLE && isfinite(dval)) {
                return dw_emit_double(ctx, dval);
            }
            /* fall through to string emission */
        }
emit_string:
        return dw_emit_string(ctx, Z_STRVAL_P(zv), Z_STRLEN_P(zv));
    }
    case IS_ARRAY: {
        bool force_object = (ctx->flags & FASTJSON_ENCODE_FORCE_OBJECT) != 0;
        /* Force copy-on-write if a nested serializer mutates this array through
         * an alias; in-place reallocation would invalidate the hash cursor. */
        zval arr_copy;
        ZVAL_COPY(&arr_copy, zv);
        bool ok = dw_emit_array(ctx, Z_ARRVAL(arr_copy), remaining_depth,
                                force_object);
        zval_ptr_dtor_nogc(&arr_copy);
        return ok;
    }
    case IS_OBJECT:
        return dw_emit_object(ctx, zv, remaining_depth);
    case IS_REFERENCE:
        ZVAL_DEREF(zv);
        return dw_encode_zval(ctx, zv, remaining_depth);
    default:
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_UNSUPPORTED_TYPE,
            "Type is not supported", false);
    }
}

static bool dw_encode_zval(fastjson_dw_ctx *ctx, zval *zv,
                           zend_long remaining_depth)
{
#if !FASTJSON_HAVE_NATIVE_STACK_LIMIT
    /* JsonSerializable and reference chains can recurse without consuming
     * JSON container depth. Keep those paths bounded on PHP/platforms where
     * Zend cannot report the native stack limit. */
    if (ctx->call_depth >= 1024) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }
    ctx->call_depth++;
    bool ok = dw_encode_zval_inner(ctx, zv, remaining_depth);
    ctx->call_depth--;
    return ok;
#else
    return dw_encode_zval_inner(ctx, zv, remaining_depth);
#endif
}

zend_string *fastjson_directwrite_encode(zval *value, zend_long flags,
                                         zend_long depth,
                                         fastjson_error_state *error_state)
{
    switch (Z_TYPE_P(value)) {
    case IS_NULL:
        fastjson_error_state_clear(error_state);
        return zend_string_init("null", 4, 0);
    case IS_FALSE:
        fastjson_error_state_clear(error_state);
        return zend_string_init("false", 5, 0);
    case IS_TRUE:
        fastjson_error_state_clear(error_state);
        return zend_string_init("true", 4, 0);
    case IS_LONG:
        fastjson_error_state_clear(error_state);
        return zend_long_to_str(Z_LVAL_P(value));
    default:
        break;
    }

    fastjson_dw_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.flags = flags;
    ctx.yflags = fastjson_translate_write_flags(flags, false);
    ctx.precision = (int)PG(serialize_precision);
    ctx.partial_output = (flags & FASTJSON_ENCODE_PARTIAL_OUTPUT_ON_ERROR) != 0;
    ctx.pretty_print = (flags & FASTJSON_ENCODE_PRETTY_PRINT) != 0;
    fastjson_error_state_clear(&ctx.error);

    smart_str_alloc(&ctx.buf, 256, 0);

    bool ok = dw_encode_zval(&ctx, value, depth);
    *error_state = ctx.error;

    if (!ok || ctx.hard_error) {
        smart_str_free(&ctx.buf);
        return NULL;
    }

    smart_str_0(&ctx.buf);
    return ctx.buf.s;
}
