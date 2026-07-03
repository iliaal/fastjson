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
 * Immutable-tree JSON Pointer set: walks a parsed yyjson_doc, emits a
 * smart_str JSON byte stream, and substitutes the value at the pointer
 * path. Avoids yyjson_doc_mut_copy + yyjson_mut_write for the common
 * edit-in-place case (large base doc, small replacement).
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>

#include "php.h"
#include "Zend/zend_smart_str.h"
#include "php_fastjson.h"
#include "fastjson_arginfo.h"
#include "fastjson_alloc.h"
#include "yyjson.h"

#define FJ_IMUT_VAL(v) ((yyjson_val *)(v))

typedef struct fj_ptr_seg {
    const char *str;
    size_t      len;
} fj_ptr_seg;

typedef struct fj_splice_ctx {
    smart_str          buf;
    yyjson_write_flag  yflags;
    bool               pretty;
    int                indent;
    const yyjson_val  *replacement;
    const fj_ptr_seg  *segs;
    size_t             nsegs;
    bool               settable;
} fj_splice_ctx;

static bool fj_splice_reserve(fj_splice_ctx *ctx, size_t add)
{
    size_t cur = ctx->buf.s ? ZSTR_LEN(ctx->buf.s) : 0;
    if (UNEXPECTED(add > ZSTR_MAX_LEN - cur)) {
        return false;
    }
    smart_str_alloc(&ctx->buf, add, 0);
    return true;
}

static inline void fj_splice_newline_indent(fj_splice_ctx *ctx, int level)
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

static bool fj_splice_write_string(fj_splice_ctx *ctx, const char *s, size_t len)
{
    if (!fj_splice_reserve(ctx, len * 6 + 2)) {
        return false;
    }
    char *cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
    char *end = yyjson_write_string_to_buf(cur, s, len, ctx->yflags);
    if (UNEXPECTED(end == NULL)) {
        return false;
    }
    ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
    return true;
}

static bool fj_splice_write_number(fj_splice_ctx *ctx, const yyjson_val *val)
{
    if (!fj_splice_reserve(ctx, 40)) {
        return false;
    }
    char *cur = ZSTR_VAL(ctx->buf.s) + ZSTR_LEN(ctx->buf.s);
    char *end = yyjson_write_number(val, cur);
    if (UNEXPECTED(end == NULL)) {
        return false;
    }
    ZSTR_LEN(ctx->buf.s) = (size_t)(end - ZSTR_VAL(ctx->buf.s));
    return true;
}

static bool fj_splice_write_raw(fj_splice_ctx *ctx, const yyjson_val *val)
{
    const char *raw = yyjson_get_raw(val);
    size_t raw_len = yyjson_get_len(val);
    if (!fj_splice_reserve(ctx, raw_len)) {
        return false;
    }
    smart_str_appendl(&ctx->buf, raw, raw_len);
    return true;
}

static bool fj_splice_write_full(fj_splice_ctx *ctx, const yyjson_val *val);
static bool fj_splice_write_array_full(fj_splice_ctx *ctx,
                                       const yyjson_val *arr);
static bool fj_splice_write_object_full(fj_splice_ctx *ctx,
                                        const yyjson_val *obj);
static bool fj_splice_write_array(fj_splice_ctx *ctx, const yyjson_val *arr,
                                  size_t seg_idx);
static bool fj_splice_write_object(fj_splice_ctx *ctx, const yyjson_val *obj,
                                   size_t seg_idx);
static bool fj_splice_write_at(fj_splice_ctx *ctx, const yyjson_val *val,
                               size_t seg_idx);
static bool fj_splice_build_suffix(fj_splice_ctx *ctx, size_t seg_idx);

static bool fj_splice_write_full(fj_splice_ctx *ctx, const yyjson_val *val)
{
    switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_NULL:
        smart_str_appendl(&ctx->buf, "null", 4);
        return true;
    case YYJSON_TYPE_BOOL:
        if (yyjson_get_bool(val)) {
            smart_str_appendl(&ctx->buf, "true", 4);
        } else {
            smart_str_appendl(&ctx->buf, "false", 5);
        }
        return true;
    case YYJSON_TYPE_NUM:
        return fj_splice_write_number(ctx, val);
    case YYJSON_TYPE_STR:
        return fj_splice_write_string(ctx, yyjson_get_str(val),
                                      yyjson_get_len(val));
    case YYJSON_TYPE_RAW:
        return fj_splice_write_raw(ctx, val);
    case YYJSON_TYPE_ARR:
        return fj_splice_write_array_full(ctx, val);
    case YYJSON_TYPE_OBJ:
        return fj_splice_write_object_full(ctx, val);
    default:
        return false;
    }
}

static bool fj_seg_is_index(const fj_ptr_seg *seg, size_t *out_idx)
{
    if (seg->len == 0) {
        return false;
    }
    size_t idx = 0;
    for (size_t i = 0; i < seg->len; i++) {
        unsigned char c = (unsigned char)seg->str[i];
        if (!isdigit(c)) {
            return false;
        }
        idx = idx * 10 + (size_t)(c - '0');
    }
    *out_idx = idx;
    return true;
}

static bool fj_seg_key_eq(yyjson_val *key, const fj_ptr_seg *seg)
{
    return yyjson_get_len(key) == seg->len
        && memcmp(yyjson_get_str(key), seg->str, seg->len) == 0;
}

static bool fj_splice_write_object_key(fj_splice_ctx *ctx,
                                       const fj_ptr_seg *seg)
{
    return fj_splice_write_string(ctx, seg->str, seg->len);
}

static bool fj_splice_write_array_full(fj_splice_ctx *ctx, const yyjson_val *arr)
{
    bool pretty = ctx->pretty;
    size_t n = yyjson_arr_size(FJ_IMUT_VAL(arr));
    bool empty = n == 0;
    smart_str_appendc(&ctx->buf, '[');
    if (pretty && !empty) {
        ctx->indent++;
    }

    bool first = true;
    yyjson_val *item;
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(FJ_IMUT_VAL(arr), &iter);
    while ((item = yyjson_arr_iter_next(&iter))) {
        if (!first) {
            smart_str_appendc(&ctx->buf, ',');
        }
        if (pretty) {
            fj_splice_newline_indent(ctx, ctx->indent);
        }
        if (!fj_splice_write_full(ctx, item)) {
            return false;
        }
        first = false;
    }

    if (pretty && !empty) {
        ctx->indent--;
        fj_splice_newline_indent(ctx, ctx->indent);
    }
    smart_str_appendc(&ctx->buf, ']');
    return true;
}

static bool fj_splice_write_object_full(fj_splice_ctx *ctx, const yyjson_val *obj)
{
    bool pretty = ctx->pretty;
    size_t n = yyjson_obj_size(FJ_IMUT_VAL(obj));
    bool empty = n == 0;
    bool body_open = false;

    smart_str_appendc(&ctx->buf, '{');
    bool first = true;
    yyjson_val *key;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(FJ_IMUT_VAL(obj), &iter);
    while ((key = yyjson_obj_iter_next(&iter))) {
        yyjson_val *child = yyjson_obj_iter_get_val(key);
        if (pretty && !body_open && !empty) {
            ctx->indent++;
            body_open = true;
        }
        if (!first) {
            smart_str_appendc(&ctx->buf, ',');
        }
        if (pretty) {
            fj_splice_newline_indent(ctx, ctx->indent);
        }
        if (!fj_splice_write_string(ctx, yyjson_get_str(key),
                                    yyjson_get_len(key))) {
            return false;
        }
        smart_str_appendc(&ctx->buf, ':');
        if (pretty) {
            smart_str_appendc(&ctx->buf, ' ');
        }
        if (!fj_splice_write_full(ctx, child)) {
            return false;
        }
        first = false;
    }
    if (body_open) {
        ctx->indent--;
        fj_splice_newline_indent(ctx, ctx->indent);
    }
    smart_str_appendc(&ctx->buf, '}');
    return true;
}

static bool fj_splice_write_array(fj_splice_ctx *ctx, const yyjson_val *arr,
                                  size_t seg_idx)
{
    if (seg_idx >= ctx->nsegs) {
        return fj_splice_write_array_full(ctx, arr);
    }

    size_t want_idx;
    if (!fj_seg_is_index(&ctx->segs[seg_idx], &want_idx)) {
        ctx->settable = false;
        return false;
    }

    size_t n = yyjson_arr_size(arr);
    if (want_idx >= n) {
        ctx->settable = false;
        return false;
    }

    bool pretty = ctx->pretty;
    bool empty = n == 0;
    smart_str_appendc(&ctx->buf, '[');
    if (pretty && !empty) {
        ctx->indent++;
    }

    bool first = true;
    size_t cur_idx = 0;
    yyjson_val *item;
    yyjson_arr_iter iter;
    yyjson_arr_iter_init(arr, &iter);
    while ((item = yyjson_arr_iter_next(&iter))) {
        if (!first) {
            smart_str_appendc(&ctx->buf, ',');
        }
        if (pretty) {
            fj_splice_newline_indent(ctx, ctx->indent);
        }
        if (cur_idx == want_idx) {
            if (seg_idx == ctx->nsegs - 1) {
                if (!fj_splice_write_full(ctx, ctx->replacement)) {
                    return false;
                }
            } else if (!fj_splice_write_at(ctx, item, seg_idx + 1)) {
                return false;
            }
        } else if (!fj_splice_write_full(ctx, item)) {
            return false;
        }
        first = false;
        cur_idx++;
    }

    if (pretty && !empty) {
        ctx->indent--;
        fj_splice_newline_indent(ctx, ctx->indent);
    }
    smart_str_appendc(&ctx->buf, ']');
    return true;
}

static bool fj_splice_write_object(fj_splice_ctx *ctx, const yyjson_val *obj,
                                   size_t seg_idx)
{
    if (seg_idx >= ctx->nsegs) {
        return fj_splice_write_object_full(ctx, obj);
    }

    const fj_ptr_seg *target = &ctx->segs[seg_idx];
    bool pretty = ctx->pretty;
    size_t n = yyjson_obj_size(obj);
    bool empty = n == 0;
    bool body_open = false;
    bool matched = false;

    smart_str_appendc(&ctx->buf, '{');

    bool first = true;
    yyjson_val *key;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(obj, &iter);
    while ((key = yyjson_obj_iter_next(&iter))) {
        yyjson_val *child = yyjson_obj_iter_get_val(key);
        bool is_target = fj_seg_key_eq(key, target);

        if (is_target) {
            matched = true;
            if (seg_idx == ctx->nsegs - 1) {
                if (pretty && !body_open) {
                    ctx->indent++;
                    body_open = true;
                }
                if (!first) {
                    smart_str_appendc(&ctx->buf, ',');
                }
                if (pretty) {
                    fj_splice_newline_indent(ctx, ctx->indent);
                }
                if (!fj_splice_write_object_key(ctx, target)) {
                    return false;
                }
                smart_str_appendc(&ctx->buf, ':');
                if (pretty) {
                    smart_str_appendc(&ctx->buf, ' ');
                }
                if (!fj_splice_write_full(ctx, ctx->replacement)) {
                    return false;
                }
                first = false;
                continue;
            }
            if (!yyjson_is_ctn(FJ_IMUT_VAL(child))) {
                ctx->settable = false;
                return false;
            }
        }

        if (pretty && !body_open && !empty) {
            ctx->indent++;
            body_open = true;
        }
        if (!first) {
            smart_str_appendc(&ctx->buf, ',');
        }
        if (pretty) {
            fj_splice_newline_indent(ctx, ctx->indent);
        }
        if (!fj_splice_write_string(ctx, yyjson_get_str(key),
                                    yyjson_get_len(key))) {
            return false;
        }
        smart_str_appendc(&ctx->buf, ':');
        if (pretty) {
            smart_str_appendc(&ctx->buf, ' ');
        }
        if (is_target) {
            if (!fj_splice_write_at(ctx, child, seg_idx + 1)) {
                return false;
            }
        } else if (!fj_splice_write_full(ctx, child)) {
            return false;
        }
        first = false;
    }

    if (!matched) {
        if (pretty && !body_open) {
            ctx->indent++;
            body_open = true;
        }
        if (!first) {
            smart_str_appendc(&ctx->buf, ',');
        }
        if (pretty) {
            fj_splice_newline_indent(ctx, ctx->indent);
        }
        if (!fj_splice_write_object_key(ctx, target)) {
            return false;
        }
        smart_str_appendc(&ctx->buf, ':');
        if (pretty) {
            smart_str_appendc(&ctx->buf, ' ');
        }
        if (seg_idx == ctx->nsegs - 1) {
            if (!fj_splice_write_full(ctx, ctx->replacement)) {
                return false;
            }
        } else if (!fj_splice_build_suffix(ctx, seg_idx)) {
            return false;
        }
    }

    if (body_open) {
        ctx->indent--;
        fj_splice_newline_indent(ctx, ctx->indent);
    }
    smart_str_appendc(&ctx->buf, '}');
    return true;
}

/* Build the JSON subtree for pointer suffix segs[seg_idx..] when parent
 * keys through segs[seg_idx-1] already exist but segs[seg_idx] does not. */
static bool fj_splice_build_suffix(fj_splice_ctx *ctx, size_t seg_idx)
{
    if (seg_idx >= ctx->nsegs) {
        return false;
    }
    if (seg_idx == ctx->nsegs - 1) {
        return fj_splice_write_full(ctx, ctx->replacement);
    }

    const fj_ptr_seg *key = &ctx->segs[seg_idx + 1];
    bool pretty = ctx->pretty;
    smart_str_appendc(&ctx->buf, '{');
    if (pretty) {
        ctx->indent++;
        fj_splice_newline_indent(ctx, ctx->indent);
    }
    if (!fj_splice_write_object_key(ctx, key)) {
        return false;
    }
    smart_str_appendc(&ctx->buf, ':');
    if (pretty) {
        smart_str_appendc(&ctx->buf, ' ');
    }
    if (!fj_splice_build_suffix(ctx, seg_idx + 1)) {
        return false;
    }
    if (pretty) {
        ctx->indent--;
        fj_splice_newline_indent(ctx, ctx->indent);
    }
    smart_str_appendc(&ctx->buf, '}');
    return true;
}

static bool fj_splice_write_at(fj_splice_ctx *ctx, const yyjson_val *val,
                               size_t seg_idx)
{
    if (seg_idx >= ctx->nsegs) {
        return fj_splice_write_full(ctx, val);
    }
    if (yyjson_is_obj(FJ_IMUT_VAL(val))) {
        return fj_splice_write_object(ctx, val, seg_idx);
    }
    if (yyjson_is_arr(FJ_IMUT_VAL(val))) {
        return fj_splice_write_array(ctx, val, seg_idx);
    }
    ctx->settable = false;
    return false;
}

static size_t fj_pointer_count_segments(const char *ptr, size_t len)
{
    if (len == 0) {
        return 0;
    }
    if (ptr[0] != '/') {
        return 0;
    }
    size_t count = 0;
    for (size_t i = 1; i <= len; i++) {
        if (i == len || ptr[i] == '/') {
            count++;
        }
    }
    return count;
}

static bool fj_pointer_fill_segments(const char *ptr, size_t len,
                                     fj_ptr_seg *segs, size_t nsegs,
                                     char *storage, size_t storage_cap,
                                     size_t *storage_used)
{
    if (len == 0 || nsegs == 0) {
        return true;
    }
    if (ptr[0] != '/') {
        return false;
    }

    size_t si = 0;
    size_t seg_start = 1;
    for (size_t i = 1; i <= len; i++) {
        if (i < len && ptr[i] != '/') {
            continue;
        }
        size_t raw_len = i - seg_start;
        size_t out_len = 0;
        for (size_t j = 0; j < raw_len; j++) {
            if (ptr[seg_start + j] == '~') {
                if (j + 1 >= raw_len) {
                    return false;
                }
                unsigned char next = (unsigned char)ptr[seg_start + j + 1];
                if (next != '0' && next != '1') {
                    return false;
                }
                if (*storage_used + 1 > storage_cap) {
                    return false;
                }
                storage[(*storage_used)++] = (next == '0') ? '~' : '/';
                out_len++;
                j++;
            } else {
                if (*storage_used + 1 > storage_cap) {
                    return false;
                }
                storage[(*storage_used)++] = ptr[seg_start + j];
                out_len++;
            }
        }
        segs[si].str = storage + (*storage_used) - out_len;
        segs[si].len = out_len;
        si++;
        seg_start = i + 1;
    }
    return si == nsegs;
}

/* Build a replacement yyjson_val from a PHP zval without mut_copy. For
 * non-scalars, reuses direct-write + parse into a small side doc. */
bool fastjson_pointer_build_replacement(zval *value, zend_long value_flags,
                                        zend_long depth,
                                        fastjson_pointer_repl *out)
{
    memset(out, 0, sizeof(*out));
    ZVAL_DEREF(value);

    switch (Z_TYPE_P(value)) {
    case IS_NULL:
        out->stack_val.tag = YYJSON_TYPE_NULL | YYJSON_SUBTYPE_NONE;
        out->repl = &out->stack_val;
        return true;
    case IS_FALSE:
        out->stack_val.tag = YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_FALSE;
        out->repl = &out->stack_val;
        return true;
    case IS_TRUE:
        out->stack_val.tag = YYJSON_TYPE_BOOL | YYJSON_SUBTYPE_TRUE;
        out->repl = &out->stack_val;
        return true;
    case IS_LONG:
        out->stack_val.tag = (uint64_t)(YYJSON_TYPE_NUM | YYJSON_SUBTYPE_SINT);
        out->stack_val.uni.i64 = (int64_t)Z_LVAL_P(value);
        out->repl = &out->stack_val;
        return true;
    case IS_STRING: {
        zend_string *str = Z_STR_P(value);
        const char *bytes = ZSTR_VAL(str);
        size_t slen = ZSTR_LEN(str);
        if (!fastjson_utf8_well_formed(bytes, slen)) {
            if (!FASTJSON_HAS_UTF8_HANDLING_FLAG(value_flags)) {
                fastjson_set_encode_error(FASTJSON_ERROR_UTF8,
                    "Malformed UTF-8 characters, possibly incorrectly encoded");
                return false;
            }
            size_t sane_len;
            out->owned_str = fastjson_sanitize_utf8(bytes, slen, value_flags,
                                                    FJ_SAN_ENCODE, &sane_len);
            unsafe_yyjson_set_tag(&out->stack_val, YYJSON_TYPE_STR,
                                  YYJSON_SUBTYPE_NONE, sane_len);
            out->stack_val.uni.str = out->owned_str;
            out->repl = &out->stack_val;
            return true;
        }
        unsafe_yyjson_set_tag(&out->stack_val, YYJSON_TYPE_STR,
                              YYJSON_SUBTYPE_NONE, slen);
        out->stack_val.uni.str = bytes;
        out->repl = &out->stack_val;
        return true;
    }
    default:
        break;
    }

    zend_string *vstr = fastjson_directwrite_encode(value, value_flags, depth);
    if (vstr == NULL) {
        return false;
    }
    yyjson_read_err verr;
    out->doc = yyjson_read_opts(ZSTR_VAL(vstr), ZSTR_LEN(vstr), 0,
                                &fastjson_php_alc, &verr);
    zend_string_release(vstr);
    if (out->doc == NULL) {
        return false;
    }
    out->repl = yyjson_doc_get_root(out->doc);
    return true;
}

zend_string *fastjson_imut_pointer_set_write(yyjson_val *root,
                                             const char *pointer,
                                             size_t pointer_len,
                                             const yyjson_val *replacement,
                                             zend_long flags,
                                             fj_splice_status *status)
{
    *status = FJ_SPLICE_OK;

    if (pointer_len == 0) {
        fj_splice_ctx ctx;
        memset(&ctx, 0, sizeof(ctx));
        ctx.yflags = fastjson_translate_write_flags(flags, true);
        ctx.pretty = (flags & FASTJSON_ENCODE_PRETTY_PRINT) != 0;
        ctx.replacement = replacement;
        smart_str_alloc(&ctx.buf, 256, 0);
        if (!fj_splice_write_full(&ctx, replacement)) {
            smart_str_free(&ctx.buf);
            *status = FJ_SPLICE_WRITE_FAIL;
            return NULL;
        }
        smart_str_0(&ctx.buf);
        return ctx.buf.s;
    }

    size_t nsegs = fj_pointer_count_segments(pointer, pointer_len);
    fj_ptr_seg *segs = nsegs ? emalloc(nsegs * sizeof(*segs)) : NULL;
    char *storage = nsegs ? emalloc(pointer_len) : NULL;
    size_t storage_used = 0;

    if (nsegs && !fj_pointer_fill_segments(pointer, pointer_len, segs, nsegs,
            storage, pointer_len, &storage_used)) {
        efree(segs);
        efree(storage);
        *status = FJ_SPLICE_SETTABLE_FAIL;
        return NULL;
    }

    fj_splice_ctx ctx;
    memset(&ctx, 0, sizeof(ctx));
    ctx.yflags = fastjson_translate_write_flags(flags, true);
    ctx.pretty = (flags & FASTJSON_ENCODE_PRETTY_PRINT) != 0;
    ctx.replacement = replacement;
    ctx.segs = segs;
    ctx.nsegs = nsegs;
    ctx.settable = true;
    smart_str_alloc(&ctx.buf, 256, 0);

    bool ok = fj_splice_write_at(&ctx, root, 0);
    efree(segs);
    efree(storage);

    if (!ok) {
        smart_str_free(&ctx.buf);
        *status = ctx.settable ? FJ_SPLICE_WRITE_FAIL : FJ_SPLICE_SETTABLE_FAIL;
        return NULL;
    }

    smart_str_0(&ctx.buf);
    return ctx.buf.s;
}