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

/*
 * Direct-write encoder: walks PHP zvals straight into a smart_str
 * buffer, emitting JSON tokens inline. One-stage replacement for the
 * (now legacy) two-stage path in fastjson_encode.c that built a
 * yyjson_mut_doc intermediate.
 *
 * Why: profiling encode on the citm_catalog / random workloads showed
 * the two-stage cost (mut_val alloc per JSON value + per-val writer
 * dispatch) dominating when per-value work is small. ext/json wins on
 * those workloads precisely because its smart_str-driven walker is
 * one-stage. Direct-write closes that structural gap.
 *
 * What we reuse from yyjson:
 *   - yyjson_write_number()         — public API, takes a stack-allocated
 *                                     yyjson_val for the number to emit
 *   - yyjson_write_string_to_buf()  — vendor patch P-003, writes a
 *                                     quoted/escaped JSON string into
 *                                     a caller buffer
 *
 * What we implement ourselves:
 *   - Container brackets/commas/colons
 *   - Recursion guards and scoped JsonSerializable/property-hook lifetimes
 *   - Pretty-print indent (when JSON_PRETTY_PRINT is set)
 *   - JSON_HEX_* substitutions (inline; no second pass needed because
 *     we own the byte stream as it's written)
 *   - JSON_NUMERIC_CHECK string-to-number coercion
 *   - JSON_PRESERVE_ZERO_FRACTION integer-valued-double handling
 *   - JSON_FORCE_OBJECT, UNESCAPED_SLASHES, UNESCAPED_UNICODE
 *   - JSON_PARTIAL_OUTPUT_ON_ERROR substitutions
 *   - JSON_THROW_ON_ERROR with the "preserve global state" contract
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <math.h>

#include "php.h"
#if PHP_VERSION_ID >= 80300 && defined(ZEND_CHECK_STACK_LIMIT)
#include "Zend/zend_call_stack.h"
#define FASTJSON_HAVE_NATIVE_STACK_LIMIT 1
#else
/* zend_call_stack_overflowed() / EG(stack_limit) are 8.3+, and even on
 * 8.3+ the function is only declared when ZEND_CHECK_STACK_LIMIT is
 * configured (php-src cannot detect stack bounds on every platform).
 * Where either is absent, the remaining_depth counter (default 512)
 * still bounds recursion, so the secondary C-stack check degrades to a
 * no-op. */
#define zend_call_stack_overflowed(limit) (0)
#define FASTJSON_HAVE_NATIVE_STACK_LIMIT 0
#endif
#include "Zend/zend_enum.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_interfaces.h"
#include "Zend/zend_smart_str.h"
#include "php_fastjson.h"
#include "fastjson_arginfo.h"
#include "fastjson_alloc.h"
#include "yyjson.h"

/* Per-call state threaded through the recursive walker. Keeping it
 * struct-pointer keeps the function signature small (one arg vs five
 * positional). */
typedef struct fastjson_dw_ctx {
    smart_str       buf;
    zend_long       flags;
    yyjson_write_flag yflags;   /* translated to yyjson's flag enum
                                 * once at top-level; reused by every
                                 * call to yyjson_write_string_to_buf */
    bool            partial_output;
    bool            pretty_print;
    bool            hard_error;
    fastjson_error_state error;
    uint32_t        call_depth;
    int             indent_level;  /* current pretty-print depth; 0 at
                                    * top level, +1 per open container */
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

/* Small strings keep yyjson's single-pass writer. At the shared threshold,
 * the large-string helper fuses printable-ASCII validation and copying; for
 * special/non-ASCII bytes below 1 MiB it falls back to yyjson's single pass,
 * while larger strings get exact-size preflight to avoid 6x reservations. */
static bool dw_reserve_string(fastjson_dw_ctx *ctx, size_t len)
{
    if (UNEXPECTED(len > (ZSTR_MAX_LEN - 2) / 6)) {
        return dw_fail_too_large(ctx);
    }
    return dw_reserve(ctx, len * 6 + 2);
}

/* Shared write-flag translation (declared in php_fastjson.h). The
 * direct-write encoder passes with_pretty=false because it emits
 * pretty-print indentation itself via smart_str; yyjson-writer callers
 * such as fastjson_pointer_set pass true so PRETTY_PRINT maps to
 * YYJSON_WRITE_PRETTY. */
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
    /* IGNORE / SUBSTITUTE: handled by fastjson-side sanitization in
     * dw_emit_string_ex before yyjson runs. We deliberately do NOT
     * pass YYJSON_WRITE_ALLOW_INVALID_UNICODE -- yyjson's behavior
     * there ("emit byte raw, escape with � if needed") doesn't
     * match ext/json's strip-or-substitute semantics. */
    return yf;
}

/* Pretty-print indent emission. ext/json uses 4 spaces per level
 * and inserts a newline + indent before each value/key inside a
 * container. Format mirrors ext/json's byte-for-byte:
 *
 *   {
 *       "key": value,
 *       "key2": [
 *           1,
 *           2
 *       ]
 *   }
 *
 * That is: newline-then-indent BEFORE each item, plus newline-then-
 * indent BEFORE the closing bracket (so the bracket is one level
 * less indented than the contents). Empty containers stay compact
 * ({} and []) -- no inner whitespace. */
static inline void dw_emit_newline_indent(fastjson_dw_ctx *ctx, int level)
{
    fastjson_append_newline_indent(&ctx->buf, level);
}

/* Apply the JSON_HEX_TAG / HEX_AMP / HEX_APOS / HEX_QUOT substitutions
 * to a string buffer region [start_pos, end_pos). Operates on the
 * smart_str's underlying byte buffer; safe because these substitutions
 * only ever lengthen the region.
 *
 * The scan walks JSON escape sequences explicitly: when we see a '\',
 * we consume the whole escape (2 bytes for short escapes, 6 for \uXXXX)
 * before deciding what to do next. Only the '\"' escape is a candidate
 * for HEX_QUOT substitution. Bare '<', '>', '&', '\'' only ever appear
 * as string content (yyjson never escapes them), so HEX_TAG/AMP/APOS
 * substitute every occurrence outside an escape body. */
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

/* Number output reservation. yyjson_write_number's documented buffer
 * contract (yyjson.h) is the floor: integers need >= 21 bytes, floats
 * need >= 40. Reserve exactly those (rounded up for the int case) rather
 * than a blanket 64 -- a tighter per-value reservation means smart_str
 * carries less transient headroom between scalars. Do NOT shrink below
 * these; yyjson writes directly into the buffer. */
/* Returns true on success, false if the caller should abort the encode.
 * On invalid UTF-8 with PARTIAL_OUTPUT, emits "null" (value site) and
 * returns true so the caller continues; without PARTIAL_OUTPUT the
 * buffer is unchanged and the function returns false. Object-key
 * callers want a different partial substitution (`""` not `null`) and
 * therefore pass is_key=true to suppress the substitution here. */
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
        /* yyjson rejected the string as invalid UTF-8. Under IGNORE /
         * SUBSTITUTE, sanitize and retry; the sanitizer's output is
         * always valid UTF-8 so the second yyjson call succeeds. We
         * only allocate the temporary buffer on this slow path, so
         * the common (valid-UTF-8) case pays nothing extra. */
        if (FASTJSON_HAS_UTF8_HANDLING_FLAG(ctx->flags)) {
            size_t sane_len;
            /* Encode-side semantics: IGNORE wins on BOTH-bits;
             * SUBSTITUTE follows the maximal-subpart advance rule. */
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
            /* Fall through to the error path; sanitization shouldn't
             * fail, but if yyjson rejects our output we still need a
             * graceful exit. */
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

/* Helper: substitute the recursive null/0 marker on PARTIAL_OUTPUT
 * paths. Always sets the error code; returns true if we should
 * continue (substitution emitted) and false if we should abort. */
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
        /* Integer key: digits + optional minus. No escape needed;
         * reuse yyjson's digit writer (as dw_emit_long does) instead of
         * snprintf, avoiding its locale/format-string overhead, and wrap
         * the written digits in quotes. */
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
                if (ctx->hard_error) {
#if PHP_VERSION_ID < 80200
                    uint32_t from = (uint32_t)((_p + 1) - __ht->arData);
#else
                    uint32_t from = HT_IS_PACKED(__ht)
                        ? (uint32_t)((_z + 1) - __ht->arPacked)
                        : (uint32_t)(((Bucket *)_z + 1) - __ht->arData);
#endif
                    (void)dw_discard_array_range(ctx, ht, from,
                        remaining_depth - 1, true);
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
                if (ctx->hard_error) {
#if PHP_VERSION_ID < 80200
                    uint32_t from = (uint32_t)((_p + 1) - __ht->arData);
#else
                    uint32_t from = HT_IS_PACKED(__ht)
                        ? (uint32_t)(__z - __ht->arPacked)
                        : (uint32_t)((Bucket *)__z - __ht->arData);
#endif
                    (void)dw_discard_array_range(ctx, ht, from,
                        remaining_depth - 1, false);
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
    /* Hold a reference across the call: jsonSerialize() may drop every
     * other reference to obj (e.g. unset()-ing the array element being
     * encoded, bug77843), which would free it out from under the engine
     * -- the VM's ZEND_FETCH_THIS then reads freed memory. Our own ref
     * keeps obj alive until we release it below, after which the object
     * is destroyed if we held the last reference. */
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

    /* The serialized result is encoded at the object's own depth, not one
     * level deeper: ext/json's serializable path adds no depth level of its
     * own (only php_json_encode_array increments encoder->depth), so a
     * jsonSerialize() returning an M-deep structure needs $depth >= M, same
     * as returning that structure directly. Passing remaining_depth - 1 here
     * rejected valid input one level too shallow. */

    /* jsonSerialize() returning $this: ext/json encodes the object's own
     * properties rather than re-entering jsonSerialize() (which would trip
     * the recursion guard). Mirror json_encoder.c's `Z_OBJ(retval) == obj`
     * special case by dropping the callback guard and encoding the property
     * view directly. The property walker applies its own cycle guard. */
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

    /* Enums (after the JsonSerializable check, matching ext/json's
     * dispatch order -- an enum may implement JsonSerializable). A
     * backed enum serializes as its backing value; a non-backed enum is
     * an error, exactly like ext/json's php_json_encode_serializable_enum
     * (which substitutes 0 under PARTIAL_OUTPUT_ON_ERROR). */
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
    /* zend_array_count is not a reliable emptiness signal for objects:
     * mangled (private/protected) keys, IS_UNDEF typed-uninit slots, and
     * hookless virtual properties all count but emit nothing. Open the
     * indented pretty body lazily on the first property actually emitted
     * (body_open) and keep it compact ({}) when none are, matching
     * json_encode, which renders a private-only object as "{}". */
    smart_str_appendc(&ctx->buf, '{');

    bool first = true;
    bool body_open = false;
    zend_string *key;
    zend_ulong index;
    zval *item;
    /* Capture the object before any property read enters userland. */
    ZEND_HASH_FOREACH_KEY_VAL(props, index, key, item) {
        /* Skip protected/private members. zend_get_properties_for with
         * PURPOSE_JSON already filters these for stdClass, but custom
         * get_properties_for handlers may still hand back the raw
         * property table. Mangled keys carry the "\0class\0name" shape;
         * filter them out the same way ext/json does. */
        if (key && ZSTR_VAL(key)[0] == '\0' && ZSTR_LEN(key) > 0) {
            continue;
        }
        /* Declared typed/untyped properties live in
         * zend_object.properties_table; the get_properties HT stores
         * IS_INDIRECT zvals pointing to them. Resolve, skip undef. */
        if (Z_TYPE_P(item) == IS_INDIRECT) {
            item = Z_INDIRECT_P(item);
        }
        /* PHP 8.4 hooked properties: zend_get_properties_for(JSON)
         * delivers them as IS_PTR pointing at a zend_property_info.
         * ext/json invokes the get hook via zend_read_property_ex and
         * serializes the returned zval; we mirror that. Keep the returned
         * zval alive until its value has been encoded. */
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
                /* A partial get hook may have written a refcounted value
                 * into hook_rv before throwing; release it. No-op when
                 * the engine returned a borrowed pointer (hook_rv stayed
                 * IS_UNDEF). */
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
            if (ctx->hard_error) {
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
                (void)dw_discard_object_props_range(ctx, obj, props, from,
                    remaining_depth - 1);
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
    if (key && ZSTR_LEN(key) > 0 && ZSTR_VAL(key)[0] == '\0') {
        return true;
    }
    if (Z_TYPE_P(item) == IS_INDIRECT) {
        item = Z_INDIRECT_P(item);
    }

    zval hook_rv;
    bool release_hook_rv = false;
    if (Z_TYPE_P(item) == IS_PTR) {
        zend_property_info *info = Z_PTR_P(item);
#if PHP_VERSION_ID >= 80400
        if ((info->flags & ZEND_ACC_VIRTUAL)
                && (!info->hooks || !info->hooks[ZEND_PROPERTY_HOOK_GET])) {
            return true;
        }
#endif
        ZVAL_UNDEF(&hook_rv);
        zval *hooked = zend_read_property_ex(info->ce, obj,
                                             info->name, true, &hook_rv);
        if (EG(exception)) {
            zval_ptr_dtor(&hook_rv);
            return false;
        }
        item = hooked;
        release_hook_rv = true;
    }

    if (Z_ISUNDEF_P(item)) {
        if (release_hook_rv) zval_ptr_dtor(&hook_rv);
        return true;
    }
    if (key && !dw_discard_string(ctx, ZSTR_VAL(key), ZSTR_LEN(key))) {
        if (release_hook_rv) zval_ptr_dtor(&hook_rv);
        return false;
    }

    ZVAL_DEREF(item);
    bool ok = dw_discard_zval(ctx, item, remaining_depth);
    if (release_hook_rv) zval_ptr_dtor(&hook_rv);
    return ok;
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
        return EG(exception) ? false : true;
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
    zend_object *obj = Z_OBJ_P(zv);
    fastjson_dw_json_guard guard;
    if (dw_json_guard_is_recursive(&guard, zv)) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_RECURSION,
            "Recursion detected", false);
    }
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

    bool ok;
    if (Z_TYPE(retval) == IS_OBJECT && Z_OBJ(retval) == obj) {
        dw_json_guard_unprotect(&guard);
        if (remaining_depth <= 0 || remaining_depth > INT_MAX) {
            ok = dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
                "Maximum stack depth exceeded", false);
        } else {
            ok = dw_discard_object_props(ctx, &retval, remaining_depth);
        }
    } else {
        ok = dw_discard_zval(ctx, &retval, remaining_depth);
        dw_json_guard_unprotect(&guard);
    }
    zval_ptr_dtor(&retval);
    OBJ_RELEASE(obj);
    return ok;
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
        if (!isfinite(Z_DVAL_P(zv))) {
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
        /* NUMERIC_CHECK: if the string parses cleanly as a numeric
         * literal, emit it as a JSON number instead. Fall back to
         * string emission for overflow-to-INF (bug64695). */
        if (ctx->flags & FASTJSON_ENCODE_NUMERIC_CHECK) {
            const char *s = Z_STRVAL_P(zv);
            size_t slen = Z_STRLEN_P(zv);
            if (slen > 0) {
                unsigned char c0 = (unsigned char)s[0];
                /* is_numeric_string() skips leading whitespace, so a
                 * space/tab/newline-prefixed numeric string ("  42") is
                 * still numeric -- let those fall through to the real
                 * parse. Only fast-reject a first byte that can never
                 * begin a numeric literal. */
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
        /* Hold a reference across the descent: a nested JsonSerializable's
         * jsonSerialize() may mutate this very array through an aliasing
         * `&`-reference (refcount 1 -> in-place realloc of arData), which
         * would dangle the ZEND_HASH_FOREACH cursor in dw_emit_array.
         * The extra ref forces copy-on-write to separate the mutation onto
         * a fresh array, leaving the iterated storage stable. Mirrors
         * ext/json's json_encode_zval IS_ARRAY guard. */
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

/* Public entry: drives the encode. Caller takes the resulting zend_string
 * and publishes `error_state` according to its own throw contract. Returns
 * NULL on unrecoverable error. */
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
    ctx.partial_output = (flags & FASTJSON_ENCODE_PARTIAL_OUTPUT_ON_ERROR) != 0;
    ctx.pretty_print = (flags & FASTJSON_ENCODE_PRETTY_PRINT) != 0;
    fastjson_error_state_clear(&ctx.error);

    /* Reserve a sensible starting buffer. Real overflow growth via
     * smart_str_alloc; this just avoids the first 2-3 growth doublings
     * for typical encodes. */
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
