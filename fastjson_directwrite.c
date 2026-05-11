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
#include "Zend/zend_call_stack.h"
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
    bool            escape_slashes;
    bool            partial_output;
    bool            pretty_print;
    int             indent_level;  /* current pretty-print depth; 0 at
                                    * top level, +1 per open container */
    HashTable       retval_stash;
    bool            retval_stash_inited;
} fastjson_dw_ctx;

static bool dw_encode_zval(fastjson_dw_ctx *ctx, zval *zv,
                           zend_long remaining_depth);

static yyjson_write_flag dw_translate_yyjson_flags(zend_long php_flags)
{
    yyjson_write_flag yf = 0;
    if (!(php_flags & FASTJSON_ENCODE_UNESCAPED_SLASHES)) {
        yf |= YYJSON_WRITE_ESCAPE_SLASHES;
    }
    if (!(php_flags & FASTJSON_ENCODE_UNESCAPED_UNICODE)) {
        yf |= YYJSON_WRITE_ESCAPE_UNICODE;
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
static void dw_apply_hex_escapes(fastjson_dw_ctx *ctx,
                                 size_t start_pos)
{
    zend_long flags = ctx->flags;
    bool tag  = flags & FASTJSON_ENCODE_HEX_TAG;
    bool amp  = flags & FASTJSON_ENCODE_HEX_AMP;
    bool apos = flags & FASTJSON_ENCODE_HEX_APOS;
    bool quot = flags & FASTJSON_ENCODE_HEX_QUOT;
    if (!(tag || amp || apos || quot)) return;

    /* Two-pass approach is cleanest given smart_str's lack of insert
     * primitives: read the original bytes into a temp, then rewrite
     * over them with substitutions applied. The smart_str_alloc up
     * front reserves worst-case space (each input byte -> 6 output). */
    size_t orig_len = ZSTR_LEN(ctx->buf.s) - start_pos;
    if (orig_len == 0) return;

    char *orig = emalloc(orig_len);
    memcpy(orig, ZSTR_VAL(ctx->buf.s) + start_pos, orig_len);

    ZSTR_LEN(ctx->buf.s) = start_pos;
    smart_str_alloc(&ctx->buf, orig_len * 6, 0);
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
}

/* Worst-case yyjson string output size for `len` source bytes:
 *     \uXXXX expansion = 6 bytes per source byte (non-ASCII chars with
 *     ESCAPE_UNICODE), plus the surrounding quotes. */
#define DW_STR_WORST(len) ((size_t)(len) * 6 + 2)

/* Number output: yyjson_write_number caps at ~32 chars for f64 worst-
 * case. 64 is generous. */
#define DW_NUM_WORST 64

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
    smart_str_alloc(&ctx->buf, DW_STR_WORST(len), 0);
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
            smart_str_alloc(&ctx->buf, DW_STR_WORST(sane_len), 0);
            cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
            end = yyjson_write_string_to_buf(cur, sane, sane_len, ctx->yflags);
            efree(sane);
            if (EXPECTED(end != NULL)) {
                ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
                dw_apply_hex_escapes(ctx, start_pos);
                return true;
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
    dw_apply_hex_escapes(ctx, start_pos);
    return true;
}

static inline bool dw_emit_string(fastjson_dw_ctx *ctx, const char *s, size_t len)
{
    return dw_emit_string_ex(ctx, s, len, false);
}

static void dw_emit_long(fastjson_dw_ctx *ctx, zend_long n)
{
    smart_str_alloc(&ctx->buf, DW_NUM_WORST, 0);
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
     * the long path. */
    if (!(ctx->flags & FASTJSON_ENCODE_PRESERVE_ZERO_FRACTION)
            && d == floor(d) && fabs(d) <= 1e15) {
        dw_emit_long(ctx, (zend_long)d);
        return true;
    }
    smart_str_alloc(&ctx->buf, DW_NUM_WORST, 0);
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
         * emit directly with surrounding quotes. */
        char buf[24];
        int len = snprintf(buf, sizeof(buf), ZEND_LONG_FMT,
                           (zend_long)index);
        smart_str_alloc(&ctx->buf, (size_t)len + 2, 0);
        smart_str_appendc(&ctx->buf, '"');
        smart_str_appendl(&ctx->buf, buf, (size_t)len);
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
    if (remaining_depth <= 0) {
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
    if (Z_IS_RECURSIVE_P(zv)) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_RECURSION,
            "Recursion detected", false);
    }
    Z_PROTECT_RECURSION_P(zv);

    zval retval;
    ZVAL_UNDEF(&retval);
    zend_call_method_with_0_params(Z_OBJ_P(zv), Z_OBJCE_P(zv), NULL,
                                   "jsonserialize", &retval);
    if (EG(exception)) {
        zval_ptr_dtor(&retval);
        Z_UNPROTECT_RECURSION_P(zv);
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
    Z_UNPROTECT_RECURSION_P(zv);
    return ok;
}

static bool dw_emit_object(fastjson_dw_ctx *ctx, zval *zv,
                           zend_long remaining_depth)
{
    if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }
    if (remaining_depth <= 0) {
        return dw_partial_or_fail(ctx, FASTJSON_ERROR_DEPTH,
            "Maximum stack depth exceeded", false);
    }

    if (fastjson_json_serializable_ce != NULL
            && instanceof_function(Z_OBJCE_P(zv),
                                   fastjson_json_serializable_ce)) {
        return dw_emit_jsonserializable(ctx, zv, remaining_depth);
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
    /* Object emptiness: zend_array_count is shallow; for objects the
     * declared-property IS_INDIRECT slots that are IS_UNDEF (typed
     * uninit) don't contribute to JSON output but DO contribute to
     * zend_array_count. Detect emptiness conservatively: assume not
     * empty unless props has 0 entries. The pretty-print contract
     * just needs us to not add inner whitespace for truly empty
     * objects; for "looks-non-empty but actually no JSON output"
     * we emit "{ \n }" which is a minor cosmetic difference. */
    bool empty = zend_array_count(props) == 0;

    smart_str_appendc(&ctx->buf, '{');
    if (pretty && !empty) ctx->indent_level++;

    bool first = true;
    zend_string *key;
    zend_ulong index;
    zval *item;
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
            zval *hooked = zend_read_property_ex(info->ce, Z_OBJ_P(zv),
                                                 info->name, false, &hook_rv);
            if (EG(exception)) {
                if (need_recursion_guard) GC_UNPROTECT_RECURSION(props);
                if (pretty && !empty) ctx->indent_level--;
                zend_release_properties(props);
                return false;
            }
            if (!ctx->retval_stash_inited) {
                zend_hash_init(&ctx->retval_stash, 0, NULL, ZVAL_PTR_DTOR, 0);
                ctx->retval_stash_inited = true;
            }
            item = zend_hash_next_index_insert(&ctx->retval_stash, hooked);
        }
        if (Z_ISUNDEF_P(item)) continue;
        ZVAL_DEREF(item);
        if (!first) smart_str_appendc(&ctx->buf, ',');
        if (pretty) dw_emit_newline_indent(ctx, ctx->indent_level);
        if (!dw_emit_object_key(ctx, key, index)
                || !dw_encode_zval(ctx, item, remaining_depth - 1)) {
            if (need_recursion_guard) GC_UNPROTECT_RECURSION(props);
            if (pretty && !empty) ctx->indent_level--;
            zend_release_properties(props);
            return false;
        }
        first = false;
    } ZEND_HASH_FOREACH_END();
    if (pretty && !empty) {
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
    ctx.yflags = dw_translate_yyjson_flags(flags);
    ctx.escape_slashes = !(flags & FASTJSON_ENCODE_UNESCAPED_SLASHES);
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
