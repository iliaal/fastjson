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
 *   - Recursion guards, retval_stash for JsonSerializable lifetime
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
#else
/* zend_call_stack_overflowed() / EG(stack_limit) are 8.3+, and even on
 * 8.3+ the function is only declared when ZEND_CHECK_STACK_LIMIT is
 * configured (php-src cannot detect stack bounds on every platform).
 * Where either is absent, the remaining_depth counter (default 512)
 * still bounds recursion, so the secondary C-stack check degrades to a
 * no-op. */
#define zend_call_stack_overflowed(limit) (0)
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
    int             indent_level;  /* current pretty-print depth; 0 at
                                    * top level, +1 per open container */
    HashTable       retval_stash;
    bool            retval_stash_inited;
} fastjson_dw_ctx;

static bool dw_encode_zval(fastjson_dw_ctx *ctx, zval *zv,
                           zend_long remaining_depth);

static bool dw_fail_too_large(void)
{
    fastjson_set_encode_error(FASTJSON_ERROR_UNSUPPORTED_TYPE,
                              "Encoded JSON string is too large");
    return false;
}

static bool dw_reserve(fastjson_dw_ctx *ctx, size_t add_len)
{
    size_t cur_len = ctx->buf.s ? ZSTR_LEN(ctx->buf.s) : 0;
    if (UNEXPECTED(add_len > ZSTR_MAX_LEN - cur_len)) {
        return dw_fail_too_large();
    }
    smart_str_alloc(&ctx->buf, add_len, 0);
    return true;
}

/* Worst-case yyjson string output size for `len` source bytes:
 * \uXXXX expansion = 6 bytes per source byte (non-ASCII chars with
 * ESCAPE_UNICODE), plus the surrounding quotes. */
static bool dw_reserve_string(fastjson_dw_ctx *ctx, size_t len)
{
    if (UNEXPECTED(len > (ZSTR_MAX_LEN - 2) / 6)) {
        return dw_fail_too_large();
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
    if (UNEXPECTED(level > 8
            && (size_t)level <= (ZSTR_MAX_LEN - 1) / 4)) {
        size_t spaces = (size_t)level * 4;
        smart_str_alloc(&ctx->buf, spaces + 1, 0);
        smart_str_appendc(&ctx->buf, '\n');
        if (spaces != 0) {
            memset(ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s), ' ', spaces);
            ZSTR_LEN(ctx->buf.s) += spaces;
        }
        return;
    }

    smart_str_appendc(&ctx->buf, '\n');
    for (int i = 0; i < level; i++) {
        smart_str_appendl(&ctx->buf, "    ", 4);
    }
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
    zend_long flags = ctx->flags;
    bool tag  = flags & FASTJSON_ENCODE_HEX_TAG;
    bool amp  = flags & FASTJSON_ENCODE_HEX_AMP;
    bool apos = flags & FASTJSON_ENCODE_HEX_APOS;
    bool quot = flags & FASTJSON_ENCODE_HEX_QUOT;
    if (!(tag || amp || apos || quot)) return true;

    size_t orig_len = ZSTR_LEN(ctx->buf.s) - start_pos;
    if (orig_len == 0) return true;

    /* Pass 1: scan in place for replacement candidates and count them.
     * Skips the alloc / copy / rewrite work entirely when the string
     * carries none (the common case: HEX flags asserted defensively on
     * payloads that don't include the substituted characters). Each
     * replacement adds exactly 5 bytes (one source byte expands to a
     * six-byte \uXXXX), so the growth bound is precise. */
    const char *src = ZSTR_VAL(ctx->buf.s) + start_pos;
    size_t hits = 0;
    for (size_t i = 0; i < orig_len; i++) {
        unsigned char c = (unsigned char)src[i];
        if (c == '\\' && i + 1 < orig_len) {
            unsigned char next = (unsigned char)src[i + 1];
            if (quot && next == '"') {
                hits++;
                i++;
                continue;
            }
            /* Skip past the JSON escape body so its trailing hex digits
             * (in \uXXXX) aren't misread as content candidates. */
            if (next == 'u' && i + 5 < orig_len) {
                i += 5;
            } else {
                i += 1;
            }
            continue;
        }
        if (tag  && (c == '<' || c == '>')) { hits++; continue; }
        if (amp  && c == '&')               { hits++; continue; }
        if (apos && c == '\'')              { hits++; continue; }
    }
    if (hits == 0) return true;

    if (UNEXPECTED(hits > (ZSTR_MAX_LEN - orig_len) / 5)) {
        return dw_fail_too_large();
    }

    /* Pass 2: rewrite over a temp copy of the region. Reserve exact
     * growth (5 extra bytes per hit) rather than the worst-case 6x. */
    char *orig = emalloc(orig_len);
    memcpy(orig, ZSTR_VAL(ctx->buf.s) + start_pos, orig_len);

    ZSTR_LEN(ctx->buf.s) = start_pos;
    if (!dw_reserve(ctx, orig_len + hits * 5)) {
        efree(orig);
        return false;
    }
    for (size_t i = 0; i < orig_len; i++) {
        unsigned char c = (unsigned char)orig[i];
        if (c == '\\' && i + 1 < orig_len) {
            unsigned char next = (unsigned char)orig[i + 1];
            if (quot && next == '"') {
                smart_str_appendl(&ctx->buf, "\\u0022", 6);
                i++;
                continue;
            }
            /* Some other JSON escape (\\, \/, \b, \f, \n, \r, \t, \uXXXX).
             * Copy it verbatim and advance past the escape body so the
             * scanner doesn't reinterpret its trailing bytes as content. */
            smart_str_appendc(&ctx->buf, '\\');
            smart_str_appendc(&ctx->buf, (char)next);
            if (next == 'u' && i + 5 < orig_len) {
                smart_str_appendl(&ctx->buf, &orig[i + 2], 4);
                i += 5;
            } else {
                i += 1;
            }
            continue;
        }
        if (tag && c == '<') {
            smart_str_appendl(&ctx->buf, "\\u003C", 6); continue;
        }
        if (tag && c == '>') {
            smart_str_appendl(&ctx->buf, "\\u003E", 6); continue;
        }
        if (amp && c == '&') {
            smart_str_appendl(&ctx->buf, "\\u0026", 6); continue;
        }
        if (apos && c == '\'') {
            smart_str_appendl(&ctx->buf, "\\u0027", 6); continue;
        }
        smart_str_appendc(&ctx->buf, (char)c);
    }
    efree(orig);
    return true;
}

/* Number output reservation. yyjson_write_number's documented buffer
 * contract (yyjson.h) is the floor: integers need >= 21 bytes, floats
 * need >= 40. Reserve exactly those (rounded up for the int case) rather
 * than a blanket 64 -- a tighter per-value reservation means smart_str
 * carries less transient headroom between scalars. Do NOT shrink below
 * these; yyjson writes directly into the buffer. */
#define DW_NUM_INT_WORST  24
#define DW_NUM_REAL_WORST 40

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
    if (!dw_reserve_string(ctx, len)) {
        return false;
    }
    char *cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
    char *end = yyjson_write_string_to_buf(cur, s, len, ctx->yflags);
    if (UNEXPECTED(end == NULL)) {
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
            if (!dw_reserve_string(ctx, sane_len)) {
                efree(sane);
                return false;
            }
            cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
            end = yyjson_write_string_to_buf(cur, sane, sane_len, ctx->yflags);
            efree(sane);
            if (EXPECTED(end != NULL)) {
                ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
                return dw_apply_hex_escapes(ctx, start_pos);
            }
            /* Fall through to the error path; sanitization shouldn't
             * fail, but if yyjson rejects our output we still need a
             * graceful exit. */
        }
        fastjson_set_encode_error(FASTJSON_ERROR_UTF8,
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
    ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
    return dw_apply_hex_escapes(ctx, start_pos);
}

static inline bool dw_emit_string(fastjson_dw_ctx *ctx, const char *s, size_t len)
{
    return dw_emit_string_ex(ctx, s, len, false);
}

static void dw_emit_long(fastjson_dw_ctx *ctx, zend_long n)
{
    smart_str_alloc(&ctx->buf, DW_NUM_INT_WORST, 0);
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
        fastjson_set_encode_error(FASTJSON_ERROR_INF_OR_NAN,
            "Inf and NaN cannot be JSON encoded");
        if (ctx->partial_output) {
            /* ext/json's substitution for INF/NaN is JSON `0` per
             * inf_nan_error.phpt -- not null. */
            smart_str_appendc(&ctx->buf, '0');
            return true;
        }
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
    smart_str_alloc(&ctx->buf, DW_NUM_REAL_WORST, 0);
    yyjson_val v;
    v.tag = (uint64_t)(YYJSON_TYPE_NUM | YYJSON_SUBTYPE_REAL);
    v.uni.f64 = d;
    char *cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
    char *end = yyjson_write_number(&v, cur);
    if (UNEXPECTED(end == NULL)) {
        /* Should be unreachable since we filtered !isfinite above. */
        fastjson_set_encode_error(FASTJSON_ERROR_INF_OR_NAN,
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
    fastjson_set_encode_error(error_code, error_msg);
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
        if (!dw_reserve(ctx, DW_NUM_INT_WORST + 2)) {
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

static bool dw_emit_array(fastjson_dw_ctx *ctx, HashTable *ht,
                          zend_long remaining_depth, bool force_object)
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
            if (!dw_encode_zval(ctx, item, remaining_depth - 1)) {
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
            if (!dw_emit_object_key(ctx, key, index)
                    || !dw_encode_zval(ctx, item, remaining_depth - 1)) {
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
    /* zv may be an interior pointer into ctx->retval_stash: when a
     * JsonSerializable's jsonSerialize() result is itself a
     * JsonSerializable object, dw_emit_object forwards the stashed slot
     * down to here. The insert below -- and any deeper insert during the
     * recursion -- can rehash and relocate the stash's arData, dangling
     * zv. The zend_object never moves, so capture it up front and drive
     * the recursion guard through obj, never re-dereferencing zv after
     * the insert. Mirrors the fix in dw_emit_object. */
    zend_object *obj = Z_OBJ_P(zv);
    if (GC_IS_RECURSIVE(obj)) {
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
    GC_PROTECT_RECURSION(obj);

    zval retval;
    ZVAL_UNDEF(&retval);
    zend_call_method_with_0_params(obj, obj->ce, NULL,
                                   "jsonserialize", &retval);
    if (EG(exception)) {
        zval_ptr_dtor(&retval);
        GC_UNPROTECT_RECURSION(obj);
        OBJ_RELEASE(obj);
        return false;
    }

    /* Stash the retval so its zend_strings outlive any uses below.
     * Same lifetime mechanism as the legacy encoder. Lazy-init keeps
     * the no-JsonSerializable-anywhere path cheap. */
    if (!ctx->retval_stash_inited) {
        zend_hash_init(&ctx->retval_stash, 0, NULL, ZVAL_PTR_DTOR, 0);
        ctx->retval_stash_inited = true;
    }
    zval *stashed = zend_hash_next_index_insert(&ctx->retval_stash, &retval);

    bool ok = dw_encode_zval(ctx, stashed, remaining_depth - 1);
    GC_UNPROTECT_RECURSION(obj);
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
    /* See dw_emit_array: a $depth above INT_MAX wraps non-positive in
     * ext/json and trips JSON_ERROR_DEPTH on every container; match it. */
    if (remaining_depth <= 0 || remaining_depth > INT_MAX) {
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

    /* Use the JSON-purpose property view so engine objects (DateTime,
     * ArrayObject, etc.) get the same property set ext/json sees, and
     * stdClass's protected/private members get filtered out at the
     * source. Must be paired with zend_release_properties() on every
     * exit path. */
    HashTable *props = zend_get_properties_for(zv, ZEND_PROP_PURPOSE_JSON);
    if (props == NULL) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_UNSUPPORTED_TYPE,
            "Object has no properties", false);
    }

    bool need_recursion_guard = !(GC_FLAGS(props) & GC_IMMUTABLE);
    if (need_recursion_guard) {
        if (GC_IS_RECURSIVE(props)) {
            zend_release_properties(props);
            return dw_partial_or_fail(ctx, FASTJSON_ERROR_RECURSION,
                "Recursion detected", false);
        }
        GC_PROTECT_RECURSION(props);
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
    /* Capture the object up front: zv may be an interior pointer into
     * ctx->retval_stash (when this object came from a JsonSerializable
     * return, dw_emit_jsonserializable passes the stashed slot down).
     * The hooked-property branch below inserts into that same stash,
     * which can rehash and relocate arData mid-loop, dangling zv. The
     * zend_object never moves, so read it once here and never deref zv
     * again inside the loop. */
    zend_object *obj = Z_OBJ_P(zv);
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
         * serializes the returned zval; we mirror that. The returned
         * zval lives on our stack (rv); stash it so the zend_string
         * inside survives until the encode finishes. */
        zval hook_rv;
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
                                                 info->name, false, &hook_rv);
            if (EG(exception)) {
                /* A partial get hook may have written a refcounted value
                 * into hook_rv before throwing; release it. No-op when
                 * the engine returned a borrowed pointer (hook_rv stayed
                 * IS_UNDEF). */
                zval_ptr_dtor(&hook_rv);
                if (need_recursion_guard) GC_UNPROTECT_RECURSION(props);
                if (body_open) ctx->indent_level--;
                zend_release_properties(props);
                return false;
            }
            if (!ctx->retval_stash_inited) {
                zend_hash_init(&ctx->retval_stash, 0, NULL, ZVAL_PTR_DTOR, 0);
                ctx->retval_stash_inited = true;
            }
            /* Copy into the stash regardless of which return shape the
             * engine used. When `hooked != &hook_rv`, the engine took the
             * trivial-read fast path and returned a borrowed pointer to
             * OBJ_PROP(...) without an addref — inserting that pointer
             * straight into a ZVAL_PTR_DTOR hash would later decrement
             * the object's own property-table refcount and trigger a UAF
             * on the next read. ZVAL_COPY uniformly addrefs; the stash's
             * dtor balances it. When `hooked == &hook_rv` the engine
             * already addref'd into hook_rv, so we release that extra
             * reference here (the copy carries the one the stash owns). */
            zval stash_copy;
            ZVAL_COPY(&stash_copy, hooked);
            zval_ptr_dtor(&hook_rv);
            item = zend_hash_next_index_insert(&ctx->retval_stash, &stash_copy);
        }
        if (Z_ISUNDEF_P(item)) continue;
        ZVAL_DEREF(item);
        if (pretty && !body_open) { ctx->indent_level++; body_open = true; }
        if (!first) smart_str_appendc(&ctx->buf, ',');
        if (pretty) dw_emit_newline_indent(ctx, ctx->indent_level);
        if (!dw_emit_object_key(ctx, key, index)
                || !dw_encode_zval(ctx, item, remaining_depth - 1)) {
            if (need_recursion_guard) GC_UNPROTECT_RECURSION(props);
            if (body_open) ctx->indent_level--;
            zend_release_properties(props);
            return false;
        }
        first = false;
    } ZEND_HASH_FOREACH_END();
    if (body_open) {
        ctx->indent_level--;
        dw_emit_newline_indent(ctx, ctx->indent_level);
    }
    smart_str_appendc(&ctx->buf, '}');

    if (need_recursion_guard) GC_UNPROTECT_RECURSION(props);
    zend_release_properties(props);
    return true;
}

static bool dw_encode_zval(fastjson_dw_ctx *ctx, zval *zv,
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
            zend_long lval;
            double dval;
            uint8_t t = is_numeric_string(Z_STRVAL_P(zv), Z_STRLEN_P(zv),
                                          &lval, &dval, 0);
            if (t == IS_LONG) {
                dw_emit_long(ctx, lval);
                return true;
            }
            if (t == IS_DOUBLE && isfinite(dval)) {
                return dw_emit_double(ctx, dval);
            }
            /* fall through to string emission */
        }
        return dw_emit_string(ctx, Z_STRVAL_P(zv), Z_STRLEN_P(zv));
    }
    case IS_ARRAY: {
        bool force_object = (ctx->flags & FASTJSON_ENCODE_FORCE_OBJECT) != 0;
        return dw_emit_array(ctx, Z_ARRVAL_P(zv), remaining_depth,
                             force_object);
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

/* Public entry: drives the encode. Caller (PHP_FUNCTION) takes the
 * resulting zend_string from the smart_str buffer. Returns NULL on
 * unrecoverable error (error code set in FASTJSON_G); caller decides
 * whether to throw or return false. */
zend_string *fastjson_directwrite_encode(zval *value, zend_long flags,
                                         zend_long depth)
{
    fastjson_dw_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.flags = flags;
    ctx.yflags = fastjson_translate_write_flags(flags, false);
    ctx.partial_output = (flags & FASTJSON_ENCODE_PARTIAL_OUTPUT_ON_ERROR) != 0;
    ctx.pretty_print = (flags & FASTJSON_ENCODE_PRETTY_PRINT) != 0;
    ctx.retval_stash_inited = false;

    /* Reserve a sensible starting buffer. Real overflow growth via
     * smart_str_alloc; this just avoids the first 2-3 growth doublings
     * for typical encodes. */
    smart_str_alloc(&ctx.buf, 256, 0);

    bool ok = dw_encode_zval(&ctx, value, depth);

    if (ctx.retval_stash_inited) {
        zend_hash_destroy(&ctx.retval_stash);
    }

    if (!ok) {
        smart_str_free(&ctx.buf);
        return NULL;
    }

    smart_str_0(&ctx.buf);
    return ctx.buf.s;
}
