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
#include <sys/stat.h>

#include "php.h"
#include "php_streams.h"
#include "ext/standard/file.h"
#include "Zend/zend_exceptions.h"
#include "Zend/zend_smart_str.h"
#include "php_fastjson.h"
#include "fastjson_arginfo.h"
#include "fastjson_alloc.h"
#include "yyjson.h"

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

static zend_long fastjson_effective_walk_depth(zend_long depth)
{
#if FASTJSON_HAVE_NATIVE_STACK_LIMIT
    return depth;
#else
    return (zend_long)fastjson_effective_stack_depth(depth);
#endif
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

#if SIZEOF_ZEND_LONG < 8
static size_t fj_write_sint64_dec(char *buf, int64_t s)
{
    if (s < 0) {
        *buf++ = '-';
        uint64_t u = (uint64_t)(-(s + 1)) + 1;
        return 1 + fj_write_uint64_dec(buf, u);
    }
    return fj_write_uint64_dec(buf, (uint64_t)s);
}
#endif

/* yyjson NUM -> zval. Real -> double; uint <= INT64_MAX -> long; a
 * larger uint becomes a decimal string under JSON_BIGINT_AS_STRING and
 * otherwise widens to double (ext/json never wraps it into a negative
 * long); sint -> long. Force-inlined for both decode walkers. */
static zend_always_inline void fj_num_to_zval(yyjson_val *val,
                                              zend_long flags, zval *out)
{
    if (yyjson_is_real(val)) {
        ZVAL_DOUBLE(out, yyjson_get_real(val));
    } else if (yyjson_is_uint(val)) {
        uint64_t u = yyjson_get_uint(val);
        if (EXPECTED(u <= (uint64_t)ZEND_LONG_MAX)) {
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
#if SIZEOF_ZEND_LONG >= 8
        ZVAL_LONG(out, (zend_long)s);
#else
        if (s >= ZEND_LONG_MIN) {
            ZVAL_LONG(out, (zend_long)s);
        } else if (flags & FASTJSON_DECODE_BIGINT_AS_STRING) {
            char buf[24];
            size_t len = fj_write_sint64_dec(buf, s);
            ZVAL_STRINGL(out, buf, len);
        } else {
            ZVAL_DOUBLE(out, (double)s);
        }
#endif
    }
}

/* yyjson RAW -> zval. yyjson emits YYJSON_TYPE_RAW under
 * YYJSON_READ_BIGNUM_AS_RAW for big integers and exponent-overflow floats. */
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

/* Fixed open-addressed table so a document of unique keys cannot grow
 * a Zend HashTable for every name. Persistently allocated per-thread so it
 * uses zero Zend request heap and is cleared lazily via used[0..count-1]. */
#define FJ_SHARE_KEY_MAX 64
#define FJ_SHARE_SLOTS 512
#define FJ_SHARE_KEY_CAP 256
#define FJ_SHARE_MISS_MARGIN 1024
#define FJ_SHARE_MIN_INPUT 4096

typedef struct {
    zend_string *str;
    zend_ulong h;
} fj_key_slot;

typedef struct {
    fj_key_slot slots[FJ_SHARE_SLOTS];
    uint16_t used[FJ_SHARE_KEY_CAP];
    uint32_t count;
    uint32_t full_hits;
    uint32_t full_misses;
    bool off;
} fj_key_cache;

typedef struct {
    fj_key_cache *share;
    zend_long flags;
    bool assoc;
} fj_walk_ctx;

static zend_always_inline zend_string *fj_key_new(const char *str,
                                                  size_t len, zend_ulong h)
{
    zend_string *s = zend_string_init(str, len, 0);
    ZSTR_H(s) = h;
    return s;
}

/* Return a borrowed zend_string* on cache hit/insert (*owned = false) so the
 * caller passes it straight to zend_hash_update without copy+release churn,
 * or a fresh owned zend_string* (*owned = true) on a full-cache miss. */
static zend_never_inline zend_string *fj_share_key_lookup(
    fj_key_cache *share, const char *str, size_t len, bool *owned)
{
    bool full = share->count >= FJ_SHARE_KEY_CAP;
    if (UNEXPECTED(full && share->full_misses
            > share->full_hits + FJ_SHARE_MISS_MARGIN)) {
        share->off = true;
        *owned = false;
        return NULL;
    }
    zend_ulong h = zend_inline_hash_func(str, len);
    uint32_t i = (uint32_t)h & (FJ_SHARE_SLOTS - 1);
    for (uint32_t n = 0; n < 8; n++) {
        fj_key_slot *slot = &share->slots[i];
        if (slot->str == NULL) {
            if (full) {
                break;
            }
            zend_string *fresh = fj_key_new(str, len, h);
            slot->str = fresh;
            slot->h = h;
            share->used[share->count++] = (uint16_t)i;
            *owned = false;
            return fresh;
        }
        if (slot->h == h && ZSTR_LEN(slot->str) == len
                && memcmp(ZSTR_VAL(slot->str), str, len) == 0) {
            share->full_hits += full;
            *owned = false;
            return slot->str;
        }
        i = (i + 1) & (FJ_SHARE_SLOTS - 1);
    }
    share->full_misses += full;
    *owned = true;
    return fj_key_new(str, len, h);
}

static zend_always_inline void fj_insert_obj_member(
    HashTable *ht, fj_key_cache *obj_share, bool assoc,
    const char *use_key, size_t use_klen, zval *child)
{
    zend_ulong idx;
    if (assoc && ZEND_HANDLE_NUMERIC_STR(use_key, use_klen, idx)) {
        zend_hash_index_update(ht, idx, child);
        return;
    }
    if (UNEXPECTED(use_klen <= 1)) {
        zend_string *zk = use_klen == 0
            ? ZSTR_EMPTY_ALLOC()
            : ZSTR_CHAR((unsigned char)use_key[0]);
        zend_hash_update(ht, zk, child);
        return;
    }
    if (obj_share != NULL && !obj_share->off && use_klen <= FJ_SHARE_KEY_MAX) {
        bool owned = false;
        zend_string *zk = fj_share_key_lookup(obj_share, use_key, use_klen,
                                              &owned);
        if (EXPECTED(zk != NULL)) {
            zend_hash_update(ht, zk, child);
            if (UNEXPECTED(owned)) {
                zend_string_release_ex(zk, 0);
            }
            return;
        }
    }
    zend_hash_str_update(ht, use_key, use_klen, child);
}

static fj_key_cache *fj_key_cache_acquire(size_t input_len)
{
    if (input_len < FJ_SHARE_MIN_INPUT || FASTJSON_G(key_cache_in_use)) {
        return NULL;
    }
    fj_key_cache *cache = (fj_key_cache *)FASTJSON_G(key_cache);
    if (UNEXPECTED(cache == NULL)) {
        cache = (fj_key_cache *)pecalloc(1, sizeof(*cache), 1);
        FASTJSON_G(key_cache) = cache;
    }
    cache->full_hits = 0;
    cache->full_misses = 0;
    cache->off = false;
    FASTJSON_G(key_cache_in_use) = true;
    return cache;
}

static void fj_key_cache_release(fj_key_cache *cache)
{
    if (cache == NULL) {
        return;
    }
    uint32_t count = cache->count;
    for (uint32_t i = 0; i < count; i++) {
        uint16_t idx = cache->used[i];
        zend_string_release_ex(cache->slots[idx].str, 0);
        cache->slots[idx].str = NULL;
    }
    cache->count = 0;
    FASTJSON_G(key_cache_in_use) = false;
}

void fastjson_key_cache_rinit(void)
{
    if (UNEXPECTED(FASTJSON_G(key_cache_in_use))) {
        fj_key_cache *cache = (fj_key_cache *)FASTJSON_G(key_cache);
        if (cache != NULL) {
            /* RSHUTDOWN did not run; previous request heap is already gone,
             * so zero slots without dereferencing stale zend_string pointers. */
            memset(cache, 0, sizeof(*cache));
        }
        FASTJSON_G(key_cache_in_use) = false;
    }
}

void fastjson_key_cache_rshutdown(void)
{
    if (UNEXPECTED(FASTJSON_G(key_cache_in_use))) {
        fj_key_cache *cache = (fj_key_cache *)FASTJSON_G(key_cache);
        fj_key_cache_release(cache);
    }
}

void fastjson_key_cache_destroy(void *cache)
{
    if (cache != NULL) {
        pefree(cache, 1);
    }
}

static bool fastjson_yyval_to_zval(const fj_walk_ctx *wctx, yyjson_val *val,
                                   zend_long remaining_depth, zval *out);
static bool fastjson_yyval_to_zval_sanitize(const fj_walk_ctx *wctx,
                                            yyjson_val *val,
                                            zend_long remaining_depth,
                                            zval *out);

/* Walker ownership and depth invariants are documented in fastjson_walk.inc. */
#define FJ_WALK_NAME fastjson_yyval_to_zval
#define FJ_WALK_LEAF_NAME fj_walk_leaf_fast
#define FJ_WALK_STR_NAME fj_walk_str_fast
#define FJ_WALK_SANITIZE 0
#include "fastjson_walk.inc"
#undef FJ_WALK_NAME
#undef FJ_WALK_LEAF_NAME
#undef FJ_WALK_STR_NAME
#undef FJ_WALK_SANITIZE

#define FJ_WALK_NAME fastjson_yyval_to_zval_sanitize
#define FJ_WALK_LEAF_NAME fj_walk_leaf_sanitize
#define FJ_WALK_STR_NAME fj_walk_str_sanitize
#define FJ_WALK_SANITIZE 1
#include "fastjson_walk.inc"
#undef FJ_WALK_NAME
#undef FJ_WALK_LEAF_NAME
#undef FJ_WALK_STR_NAME
#undef FJ_WALK_SANITIZE

/* True when json[pos] starts `\u` plus four hex digits. yyjson accepts
 * every such escape except a surrogate it cannot pair, so a read error
 * there is an unpaired surrogate. */
static bool fastjson_escape_is_surrogate(const char *json, size_t json_len,
                                         size_t pos)
{
    if (json_len - pos < 6 || json[pos + 1] != 'u') {
        return false;
    }
    for (size_t i = pos + 2; i < pos + 6; i++) {
        unsigned char c = (unsigned char)json[i];
        if (!((c >= '0' && c <= '9') || ((c | 0x20) >= 'a' && (c | 0x20) <= 'f'))) {
            return false;
        }
    }
    return true;
}

static yyjson_read_flag fastjson_translate_read_flags(
    zend_long flags, yyjson_read_flag extra_yflags)
{
    yyjson_read_flag yflags = extra_yflags;
    if (flags & FASTJSON_DECODE_BIGINT_AS_STRING) {
        yflags |= YYJSON_READ_BIGNUM_AS_RAW;
    }
    if (flags & (FASTJSON_DECODE_INVALID_UTF8_IGNORE
                 | FASTJSON_DECODE_INVALID_UTF8_SUBSTITUTE)) {
        yflags |= YYJSON_READ_ALLOW_INVALID_UNICODE;
    }
    if (flags & FASTJSON_DECODE_RELAXED) {
        yflags |= YYJSON_READ_ALLOW_COMMENTS
                | YYJSON_READ_ALLOW_TRAILING_COMMAS
                | YYJSON_READ_ALLOW_BOM;
    }
    return yflags;
}

static void fastjson_reclassify_read_err(const char *json, size_t json_len,
                                         yyjson_read_err *err)
{
    if (err->pos < json_len
            && (unsigned char)json[err->pos] >= 0x80
            && !fastjson_byte_is_valid_utf8_start(json, json_len, err->pos)) {
        err->code = YYJSON_READ_ERROR_INVALID_STRING;
    }
    if (err->code == YYJSON_READ_ERROR_INVALID_STRING
            && err->pos < json_len && json[err->pos] == '\\'
            && !fastjson_escape_is_surrogate(json, json_len, err->pos)) {
        err->code = YYJSON_READ_ERROR_UNEXPECTED_CHARACTER;
    }
}

yyjson_doc *fastjson_read_doc_alc(const char *json, size_t json_len,
                                  zend_long flags,
                                  yyjson_read_flag extra_yflags,
                                  const yyjson_alc *alc,
                                  yyjson_read_err *err)
{
    yyjson_read_flag yflags = fastjson_translate_read_flags(flags, extra_yflags);

    yyjson_doc *doc = yyjson_read_opts((char *)json, json_len, yflags,
                                       alc, err);
    if (UNEXPECTED(doc == NULL && err->code == YYJSON_READ_ERROR_INVALID_NUMBER
            && !fastjson_input_has_inf_nan_literal(json, json_len,
                    (flags & FASTJSON_DECODE_RELAXED) != 0))) {
        doc = yyjson_read_opts((char *)json, json_len,
                               yflags | YYJSON_READ_ALLOW_INF_AND_NAN,
                               alc, err);
    }
    if (UNEXPECTED(doc == NULL)) {
        fastjson_reclassify_read_err(json, json_len, err);
    }
    return doc;
}

yyjson_doc *fastjson_read_doc_ex(const char *json, size_t json_len,
                                 zend_long flags,
                                 yyjson_read_flag extra_yflags,
                                 yyjson_read_err *err)
{
    return fastjson_read_doc_alc(json, json_len, flags, extra_yflags,
                                 &fastjson_php_alc, err);
}

/* Parse and report a failure under the caller's throw contract. The caller
 * still owns input buffers and any previously parsed documents. */
static yyjson_doc *fastjson_read_doc_with_alc(
    const char *json, size_t json_len, zend_long flags,
    yyjson_read_flag extra_yflags, const yyjson_alc *alc,
    bool throw_mode, const fastjson_error_state *saved_err)
{
    yyjson_read_err err;
    yyjson_doc *doc = fastjson_read_doc_alc(json, json_len, flags,
                                            extra_yflags, alc, &err);
    if (UNEXPECTED(doc == NULL)) {
        if (throw_mode) {
            fastjson_throw_read_error(&err, saved_err);
        } else {
            fastjson_set_read_error(json, json_len, &err);
        }
    }
    return doc;
}

static yyjson_doc *fastjson_read_doc(const char *json, size_t json_len,
                                     zend_long flags,
                                     yyjson_read_flag extra_yflags,
                                     bool throw_mode,
                                     const fastjson_error_state *saved_err)
{
    return fastjson_read_doc_with_alc(json, json_len, flags, extra_yflags,
                                      &fastjson_php_alc, throw_mode, saved_err);
}

/* Convert a resolved/merged immutable subtree into return_value.
 * Frees `doc`. Returns true on success (caller clears error on the
 * non-throw path), false if the walk hit the depth cap (return_value
 * left NULL; exception thrown under throw_mode). */
static bool fastjson_walk_doc_into(yyjson_doc *doc, yyjson_val *root,
                                   bool use_assoc, zend_long depth,
                                   zend_long flags, bool throw_mode,
                                   const fastjson_error_state *saved_err,
                                   zval *return_value)
{
    zend_long walk_depth = fastjson_effective_walk_depth(depth);
    fj_key_cache *cache = fj_key_cache_acquire(yyjson_doc_get_read_size(doc));
    fj_walk_ctx wctx = { cache, flags, use_assoc };
    bool walk_ok = UNEXPECTED(FASTJSON_HAS_UTF8_HANDLING_FLAG(flags))
        ? fastjson_yyval_to_zval_sanitize(&wctx, root, walk_depth, return_value)
        : fastjson_yyval_to_zval(&wctx, root, walk_depth, return_value);
    fj_key_cache_release(cache);
    if (UNEXPECTED(!walk_ok)) {
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

/* Walk an already-parsed doc into return_value and free it.
 * $depth must already be validated. */
static void fastjson_finish_decode(yyjson_doc *doc, bool use_assoc,
                                   zend_long depth, zend_long flags,
                                   bool throw_mode,
                                   const fastjson_error_state *saved_err,
                                   zval *return_value)
{
    if (!fastjson_walk_doc_into(doc, yyjson_doc_get_root(doc), use_assoc,
                                depth, flags, throw_mode, saved_err,
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

static void fastjson_decode_impl(const char *json, size_t json_len,
                                 bool assoc, bool assoc_is_null,
                                 zend_long depth, zend_long flags,
                                 zval *return_value)
{
    bool throw_mode;
    fastjson_error_state saved_err;
    if (!fastjson_entry_prologue(flags, FASTJSON_DECODE_THROW_ON_ERROR,
                                 depth, 3, &throw_mode, &saved_err)) {
        RETURN_THROWS();
    }

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    fastjson_small_alc small_alc;
    const yyjson_alc *alc;
    if (json_len <= FASTJSON_SMALL_ALC_THRESHOLD) {
        fastjson_small_alc_init(&small_alc);
        alc = &small_alc.alc;
    } else {
        alc = &fastjson_php_alc;
    }

    yyjson_doc *doc = fastjson_read_doc_with_alc(
        json, json_len, flags, 0, alc, throw_mode, &saved_err);
    if (UNEXPECTED(doc == NULL)) {
        if (throw_mode) {
            RETURN_THROWS();
        }
        RETURN_NULL();
    }
    fastjson_finish_decode(doc, use_assoc, depth, flags, throw_mode,
                           &saved_err, return_value);
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

    fastjson_decode_impl(json, json_len, assoc, assoc_is_null, depth, flags,
                         return_value);
}

#if PHP_VERSION_ID >= 80400
ZEND_FRAMELESS_FUNCTION(fastjson_decode, 1)
{
    zval json_tmp;
    zend_string *json;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    fastjson_decode_impl(ZSTR_VAL(json), ZSTR_LEN(json), false, true, 512, 0,
                         return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
}

ZEND_FRAMELESS_FUNCTION(fastjson_decode, 2)
{
    zval json_tmp;
    zend_string *json;
    bool assoc = false;
    bool assoc_is_null = true;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    if (UNEXPECTED(!zend_parse_arg_bool_ex(arg2, &assoc, &assoc_is_null,
            /* check_null */ true, 2, /* frameless */ true))) {
        zend_wrong_parameter_type_error(2, Z_EXPECTED_BOOL_OR_NULL, arg2);
        goto flf_clean;
    }
    fastjson_decode_impl(ZSTR_VAL(json), ZSTR_LEN(json), assoc, assoc_is_null,
                         512, 0, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
}
#endif

static zend_string *fastjson_read_stream(php_stream *stream, bool *read_failed)
{
    zend_string *contents;
    if (php_stream_is(stream, PHP_STREAM_IS_STDIO)) {
        php_stream_set_option(stream, PHP_STREAM_OPTION_READ_BUFFER,
                              PHP_STREAM_BUFFER_NONE, NULL);
        contents = php_stream_copy_to_mem(stream, PHP_STREAM_COPY_ALL, 0);
        *read_failed = !php_stream_eof(stream);
        if (contents == NULL) {
            contents = ZSTR_EMPTY_ALLOC();
        }
    } else {
        smart_str read_buf = {0};
        char chunk[8192];
        while (true) {
            ssize_t got = php_stream_read(stream, chunk, sizeof(chunk));
            if (got > 0) {
                smart_str_appendl(&read_buf, chunk, (size_t)got);
                continue;
            }
            if (got < 0 || !php_stream_eof(stream)) {
                *read_failed = true;
            }
            break;
        }
        if (read_buf.s == NULL) {
            contents = ZSTR_EMPTY_ALLOC();
        } else {
            smart_str_0(&read_buf);
            contents = read_buf.s;
        }
    }
    return contents;
}

/* Reads and closes stream. Resource references and NO_FCLOSE do not block
 * pclose(), so stream destruction is deferred during read callbacks until
 * the owned close here; a callback bailout closes the stream and re-raises. */
static zend_string *fastjson_read_and_close_stream(php_stream *stream, bool *read_failed)
{
    struct { zend_string *contents; } r = { NULL };
    stream->in_free = 1;
    uint32_t no_fclose = stream->flags & PHP_STREAM_FLAG_NO_FCLOSE;
    stream->flags |= PHP_STREAM_FLAG_NO_FCLOSE;
    volatile bool bailed = false;
    zend_try {
        r.contents = fastjson_read_stream(stream, read_failed);
    } zend_catch {
        bailed = true;
    } zend_end_try();
    stream->flags = (stream->flags & ~PHP_STREAM_FLAG_NO_FCLOSE) | no_fclose;
    stream->in_free = 0;
    php_stream_close(stream);
    if (bailed) {
        zend_bailout();
    }
    return r.contents;
}

/* Zero-copy INSITU reader for regular unfiltered stdio files. Returns a
 * YYJSON_PADDING_SIZE-padded emalloc buffer (caller frees), or NULL on I/O
 * error (*read_failed = true). Keeps `stream` open so the rare parse-error /
 * overflow-retry path can rewind and recover exact pre-INSITU bytes. */
static char *fastjson_read_regular_padded(php_stream *stream, size_t size_hint,
                                          size_t *out_len, bool *read_failed)
{
    php_stream_set_option(stream, PHP_STREAM_OPTION_READ_BUFFER,
                          PHP_STREAM_BUFFER_NONE, NULL);
    /* Reserve size_hint + 1 so the trailing 0-byte EOF read fits without
     * triggering a 2x realloc on an unchanging regular file. */
    size_t cap = size_hint < SIZE_MAX - YYJSON_PADDING_SIZE - 1
        ? size_hint + 1 : size_hint;
    if (UNEXPECTED(cap > SIZE_MAX - YYJSON_PADDING_SIZE)) {
        *read_failed = true;
        return NULL;
    }
#ifdef ZEND_HAS_OOM
    char *buf = (char *)femalloc(cap + YYJSON_PADDING_SIZE, true);
    if (UNEXPECTED(buf == NULL)) {
        *read_failed = true;
        return NULL;
    }
#else
    char *buf = (char *)emalloc(cap + YYJSON_PADDING_SIZE);
#endif
    size_t len = 0;
    while (true) {
        if (UNEXPECTED(fastjson_oom_pending())) {
            efree(buf);
            *read_failed = true;
            return NULL;
        }
        if (len == cap) {
            size_t next_cap = cap < ((size_t)4 << 20) ? cap * 2 : cap + ((size_t)4 << 20);
            if (UNEXPECTED(next_cap <= cap || next_cap > SIZE_MAX - YYJSON_PADDING_SIZE)) {
                efree(buf);
                *read_failed = true;
                return NULL;
            }
#ifdef ZEND_HAS_OOM
            char *next_buf = (char *)ferealloc(buf, next_cap + YYJSON_PADDING_SIZE, true);
            if (UNEXPECTED(next_buf == NULL)) {
                efree(buf);
                *read_failed = true;
                return NULL;
            }
            buf = next_buf;
#else
            buf = (char *)erealloc(buf, next_cap + YYJSON_PADDING_SIZE);
#endif
            cap = next_cap;
        }
        ssize_t got = php_stream_read(stream, buf + len, cap - len);
        if (got > 0) {
            len += (size_t)got;
            continue;
        }
        if (got < 0 || !php_stream_eof(stream)) {
            efree(buf);
            *read_failed = true;
            return NULL;
        }
        break;
    }
    memset(buf + len, 0, YYJSON_PADDING_SIZE);
    *out_len = len;
    return buf;
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

    bool throw_mode;
    fastjson_error_state saved_err;
    if (!fastjson_entry_prologue(flags, FASTJSON_DECODE_THROW_ON_ERROR,
                                 depth, 3, &throw_mode, &saved_err)) {
        RETURN_THROWS();
    }
    fastjson_error_state operation_err;
    fastjson_save_error_state(&operation_err);

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    php_stream_context *context = php_stream_context_from_zval(NULL, 0);
    php_stream *stream = php_stream_open_wrapper_ex(path, "rb", 0, NULL,
                                                    context);
    if (stream == NULL) {
        if (EG(exception)) {
            fastjson_restore_error_state(throw_mode ? &saved_err : &operation_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "Failed to open file for reading");
        RETURN_NULL();
    }

    /* Zero-copy INSITU fast path for regular unfiltered stdio files: avoids
     * yyjson's full-file copy, halving peak memory on large files. */
    php_stream_statbuf ssb;
    if (php_stream_is(stream, PHP_STREAM_IS_STDIO)
            && stream->readfilters.head == NULL
            && php_stream_stat(stream, &ssb) == 0
            && S_ISREG(ssb.sb.st_mode)
            && ssb.sb.st_size >= 0
            && (uint64_t)ssb.sb.st_size <= (uint64_t)(SIZE_MAX - YYJSON_PADDING_SIZE)) {
        bool read_failed = false;
        size_t len = 0;
        char *buf = fastjson_read_regular_padded(
            stream, (size_t)ssb.sb.st_size, &len, &read_failed);
        if (UNEXPECTED(read_failed || buf == NULL)) {
            php_stream_close(stream);
            if (buf != NULL) efree(buf);
            if (UNEXPECTED(fastjson_oom_pending())) {
                RETURN_NULL();
            }
            fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                    "Failed to read file");
            RETURN_NULL();
        }
        yyjson_read_flag yflags = fastjson_translate_read_flags(
            flags, YYJSON_READ_INSITU);
        yyjson_read_err err;
        yyjson_doc *doc = yyjson_read_opts(buf, len, yflags,
                                           &fastjson_php_alc, &err);
        if (EXPECTED(doc != NULL)) {
            php_stream_close(stream);
            /* doc points strings directly into buf; keep buf alive until
             * fastjson_finish_decode frees doc. */
            fastjson_finish_decode(doc, use_assoc, depth, flags, throw_mode,
                                   &saved_err, return_value);
            efree(buf);
            if (throw_mode && !EG(exception)) {
                fastjson_restore_error_state(&saved_err);
            }
            return;
        }
        /* Rare error or 1e309 overflow retry: INSITU may have mutated buf in
         * place, so rewind and re-read clean bytes for exact retry/location. */
        efree(buf);
        if (UNEXPECTED(fastjson_oom_pending())) {
            php_stream_close(stream);
            RETURN_NULL();
        }
        if (php_stream_rewind(stream) == 0) {
            bool retry_failed = false;
            zend_string *contents = fastjson_read_and_close_stream(
                stream, &retry_failed);
            if (retry_failed) {
                zend_string_release(contents);
                fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                        "Failed to read file");
                RETURN_NULL();
            }
            doc = fastjson_read_doc(ZSTR_VAL(contents), ZSTR_LEN(contents),
                                    flags, 0, throw_mode, &saved_err);
            if (doc == NULL) {
                zend_string_release(contents);
                if (throw_mode) {
                    RETURN_THROWS();
                }
                RETURN_NULL();
            }
            zend_string_release(contents);
            fastjson_finish_decode(doc, use_assoc, depth, flags, throw_mode,
                                   &saved_err, return_value);
            if (throw_mode && !EG(exception)) {
                fastjson_restore_error_state(&saved_err);
            }
            return;
        }
        php_stream_close(stream);
        if (throw_mode) {
            fastjson_throw_read_error(&err, &saved_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(fastjson_translate_read_code(err.code), err.msg);
        RETURN_NULL();
    }

    bool read_failed = false;
    zend_string *contents = fastjson_read_and_close_stream(stream, &read_failed);
    if (EG(exception)) {
        zend_string_release(contents);
        fastjson_restore_error_state(throw_mode ? &saved_err : &operation_err);
        RETURN_THROWS();
    }
    if (read_failed) {
        zend_string_release(contents);
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "Failed to read file");
        RETURN_NULL();
    }

    yyjson_doc *doc = fastjson_read_doc(ZSTR_VAL(contents), ZSTR_LEN(contents),
                                        flags, 0, throw_mode, &saved_err);
    if (doc == NULL) {
        zend_string_release(contents);
        if (throw_mode) {
            RETURN_THROWS();
        }
        RETURN_NULL();
    }
    zend_string_release(contents);
    fastjson_finish_decode(doc, use_assoc, depth, flags, throw_mode,
                           &saved_err, return_value);
    if (throw_mode && !EG(exception)) {
        fastjson_restore_error_state(&saved_err);
    }
}

/* Splice-status to error-code/message mapping for pointer_set and the
 * ambiguous-target paths of pointer_get / pointer_exists. */
typedef struct {
    fj_splice_status status;
    zend_long err_code;
    const char *err_msg;
} fj_splice_error_entry;

static const fj_splice_error_entry fj_splice_errors[] = {
    { FJ_SPLICE_DEPTH_FAIL,    FASTJSON_ERROR_DEPTH,            "Maximum stack depth exceeded" },
    { FJ_SPLICE_TOO_LARGE,     FASTJSON_ERROR_UNSUPPORTED_TYPE, "Encoded JSON string is too large" },
    { FJ_SPLICE_INF_OR_NAN,    FASTJSON_ERROR_INF_OR_NAN,       "Inf and NaN cannot be JSON encoded" },
    { FJ_SPLICE_INVALID_UTF8,  FASTJSON_ERROR_UTF8,             "Malformed UTF-8 characters, possibly incorrectly encoded" },
    { FJ_SPLICE_AMBIGUOUS,     FASTJSON_ERROR_SYNTAX,           "JSON pointer target is ambiguous because the object contains duplicate members" },
    { FJ_SPLICE_SETTABLE_FAIL, FASTJSON_ERROR_SYNTAX,           "JSON pointer does not resolve to a settable location" },
};

/* Dispatch a splice failure: set error state, or throw under throw_mode.
 * Returns true if the caller should RETURN_THROWS. */
static bool fj_splice_dispatch_error(fj_splice_status status,
                                     bool throw_mode,
                                     const fastjson_error_state *saved_err)
{
    for (size_t i = 0; i < sizeof(fj_splice_errors) / sizeof(fj_splice_errors[0]); i++) {
        if (fj_splice_errors[i].status == status) {
            if (throw_mode) {
                fastjson_throw_error(fj_splice_errors[i].err_code,
                                     fj_splice_errors[i].err_msg, NULL,
                                     saved_err);
                return true;
            }
            fastjson_set_error_code(fj_splice_errors[i].err_code,
                                    fj_splice_errors[i].err_msg);
            return false;
        }
    }
    if (throw_mode) {
        fastjson_throw_error(FASTJSON_ERROR_SYNTAX,
                             "fastjson_pointer_set write failed", NULL,
                             saved_err);
        return true;
    }
    fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                            "fastjson_pointer_set write failed");
    return false;
}

static void fastjson_pointer_get_impl(const char *json, size_t json_len,
                                      const char *pointer, size_t pointer_len,
                                      bool assoc, bool assoc_is_null,
                                      zend_long depth, zend_long flags,
                                      zval *return_value)
{
    bool throw_mode;
    fastjson_error_state saved_err;
    if (!fastjson_entry_prologue(flags, FASTJSON_DECODE_THROW_ON_ERROR,
                                 depth, 4, &throw_mode, &saved_err)) {
        RETURN_THROWS();
    }

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    fastjson_small_alc small_alc;
    const yyjson_alc *alc;
    if (json_len <= FASTJSON_SMALL_ALC_THRESHOLD) {
        fastjson_small_alc_init(&small_alc);
        alc = &small_alc.alc;
    } else {
        alc = &fastjson_php_alc;
    }

    yyjson_doc *doc = fastjson_read_doc_with_alc(
        json, json_len, flags, 0, alc, throw_mode, &saved_err);
    if (UNEXPECTED(doc == NULL)) {
        if (throw_mode) {
            RETURN_THROWS();
        }
        RETURN_NULL();
    }

    yyjson_val *target;
    size_t resolve_depth = (size_t)fastjson_effective_walk_depth(depth);
    fj_splice_status pointer_status = fastjson_pointer_resolve_limited(
        yyjson_doc_get_root(doc), pointer, pointer_len, resolve_depth,
        flags, &target);
    if (pointer_status == FJ_SPLICE_AMBIGUOUS
            || pointer_status == FJ_SPLICE_DEPTH_FAIL) {
        yyjson_doc_free(doc);
        if (fj_splice_dispatch_error(pointer_status, throw_mode,
                                     &saved_err)) {
            RETURN_THROWS();
        }
        RETURN_NULL();
    }
    if (target == NULL) {
        yyjson_doc_free(doc);
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

    fastjson_pointer_get_impl(json, json_len, pointer, pointer_len,
                              assoc, assoc_is_null, depth, flags, return_value);
}

#if PHP_VERSION_ID >= 80400
ZEND_FRAMELESS_FUNCTION(fastjson_pointer_get, 2)
{
    zval json_tmp, ptr_tmp;
    zend_string *json, *ptr;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    Z_FLF_PARAM_STR(2, ptr, ptr_tmp);
    fastjson_pointer_get_impl(ZSTR_VAL(json), ZSTR_LEN(json),
                              ZSTR_VAL(ptr), ZSTR_LEN(ptr),
                              false, true, 512, 0, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
    Z_FLF_PARAM_FREE_STR(2, ptr_tmp);
}

ZEND_FRAMELESS_FUNCTION(fastjson_pointer_get, 3)
{
    zval json_tmp, ptr_tmp;
    zend_string *json, *ptr;
    bool assoc = false;
    bool assoc_is_null = true;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    Z_FLF_PARAM_STR(2, ptr, ptr_tmp);
    if (UNEXPECTED(!zend_parse_arg_bool_ex(arg3, &assoc, &assoc_is_null,
            /* check_null */ true, 3, /* frameless */ true))) {
        zend_wrong_parameter_type_error(3, Z_EXPECTED_BOOL_OR_NULL, arg3);
        goto flf_clean;
    }
    fastjson_pointer_get_impl(ZSTR_VAL(json), ZSTR_LEN(json),
                              ZSTR_VAL(ptr), ZSTR_LEN(ptr),
                              assoc, assoc_is_null, 512, 0, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
    Z_FLF_PARAM_FREE_STR(2, ptr_tmp);
}
#endif

typedef struct {
    yyjson_val *key;
    yyjson_val *value;
    bool emitted;
} fj_merge_member;

typedef struct {
    HashTable by_key;
    fj_merge_member *members;
    size_t count;
} fj_merge_index;

static bool fj_merge_walk_val(const fj_walk_ctx *wctx, yyjson_val *val,
                              zend_long remaining_depth, zval *out)
{
    return UNEXPECTED(FASTJSON_HAS_UTF8_HANDLING_FLAG(wctx->flags))
        ? fastjson_yyval_to_zval_sanitize(wctx, val, remaining_depth, out)
        : fastjson_yyval_to_zval(wctx, val, remaining_depth, out);
}

static bool fastjson_merge_patch_to_zval(const fj_walk_ctx *wctx,
                                         yyjson_val *orig,
                                         yyjson_val *patch,
                                         zend_long remaining_depth,
                                         zval *out);

static bool fj_merge_emit_key_val(const fj_walk_ctx *wctx, HashTable *ht,
                                  fj_key_cache *obj_share, yyjson_val *key,
                                  zval *child)
{
    const char *kstr = yyjson_get_str(key);
    size_t klen = yyjson_get_len(key);
    const char *use_key = kstr;
    size_t use_klen = klen;
    bool key_owned = false;
    if (UNEXPECTED(FASTJSON_HAS_UTF8_HANDLING_FLAG(wctx->flags)
            && (yyjson_get_tag(key) & YYJSON_RESERVED_INVALID_UNICODE))) {
        use_key = fastjson_sanitize_utf8(kstr, klen, wctx->flags,
                                         FJ_SAN_DECODE, &use_klen);
        if (UNEXPECTED(fastjson_oom_pending())) {
            efree((char *)use_key);
            zval_ptr_dtor(child);
            return false;
        }
        key_owned = true;
    }
    if (!wctx->assoc && UNEXPECTED(use_klen > 0 && use_key[0] == '\0')) {
        if (key_owned) efree((char *)use_key);
        zval_ptr_dtor(child);
        fastjson_set_error_code(FASTJSON_ERROR_INVALID_PROPERTY_NAME,
            "The decoded property name is invalid");
        return false;
    }
    fj_insert_obj_member(ht, obj_share, wctx->assoc, use_key, use_klen, child);
    if (key_owned) efree((char *)use_key);
    return true;
}

static bool fj_merge_has_duplicate_keys(yyjson_val *object)
{
    if (!yyjson_is_obj(object)) {
        return false;
    }
    yyjson_val *seen[16];
    size_t seen_count = 0;
    yyjson_val *key;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(object, &iter);
    while ((key = yyjson_obj_iter_next(&iter))) {
        for (size_t i = 0; i < seen_count; i++) {
            if (yyjson_get_len(seen[i]) == yyjson_get_len(key)
                    && memcmp(yyjson_get_str(seen[i]), yyjson_get_str(key),
                              yyjson_get_len(key)) == 0) {
                return true;
            }
        }
        seen[seen_count++] = key;
    }
    return false;
}

static void fj_merge_index_init(fj_merge_index *index, yyjson_val *object)
{
    size_t capacity = yyjson_is_obj(object) ? yyjson_obj_size(object) : 0;
    zend_hash_init(&index->by_key, (uint32_t)capacity, NULL, NULL, 0);
    index->members = capacity
        ? safe_emalloc(capacity, sizeof(*index->members), 0) : NULL;
    index->count = 0;

    if (!yyjson_is_obj(object)) {
        return;
    }

    yyjson_val *key;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(object, &iter);
    while ((key = yyjson_obj_iter_next(&iter))) {
        if (UNEXPECTED(fastjson_oom_pending())) {
            break;
        }
        const char *str = yyjson_get_str(key);
        size_t len = yyjson_get_len(key);
        fj_merge_member *member = zend_hash_str_find_ptr(
            &index->by_key, str, len);
        if (member == NULL) {
            member = &index->members[index->count++];
            member->key = key;
            member->emitted = false;
            zend_hash_str_add_ptr(&index->by_key, str, len, member);
        }
        member->value = yyjson_obj_iter_get_val(key);
    }
}

static void fj_merge_index_destroy(fj_merge_index *index)
{
    zend_hash_destroy(&index->by_key);
    if (index->members != NULL) {
        efree(index->members);
    }
}

static HashTable *fj_merge_alloc_container(bool assoc, size_t hint, zval *out)
{
    if (assoc) {
        array_init_size(out, (uint32_t)hint);
        return Z_ARRVAL_P(out);
    }
    object_init(out);
    if (hint == 0) {
        return NULL;
    }
    HashTable *ht = zend_new_array((uint32_t)hint);
    zend_hash_real_init_mixed(ht);
    Z_OBJ_P(out)->properties = ht;
    return ht;
}

static bool fastjson_merge_patch_indexed_to_zval(const fj_walk_ctx *wctx,
                                                 yyjson_val *orig,
                                                 yyjson_val *patch,
                                                 zend_long remaining_depth,
                                                 zval *out)
{
    fj_merge_index orig_index, patch_index;
    fj_merge_index_init(&orig_index, orig);
    fj_merge_index_init(&patch_index, patch);
    if (UNEXPECTED(fastjson_oom_pending())) {
        fj_merge_index_destroy(&patch_index);
        fj_merge_index_destroy(&orig_index);
        ZVAL_NULL(out);
        return false;
    }

    size_t hint = orig_index.count + patch_index.count;
    HashTable *ht = fj_merge_alloc_container(wctx->assoc, hint, out);
    fj_key_cache *obj_share = (hint <= 64 && wctx->share != NULL
                               && !wctx->share->off)
        ? wctx->share : NULL;
    zend_long child_depth = remaining_depth - 1;
    bool ok = true;

    for (size_t i = 0; i < orig_index.count; i++) {
        if (UNEXPECTED(fastjson_oom_pending())) {
            ok = false;
            break;
        }
        fj_merge_member *member = &orig_index.members[i];
        const char *key_str = yyjson_get_str(member->key);
        size_t key_len = yyjson_get_len(member->key);
        fj_merge_member *patch_member = zend_hash_str_find_ptr(
            &patch_index.by_key, key_str, key_len);
        if (patch_member != NULL) {
            patch_member->emitted = true;
            if (yyjson_is_null(patch_member->value)) {
                continue;
            }
        }
        zval child;
        bool child_ok = patch_member == NULL
            ? fj_merge_walk_val(wctx, member->value, child_depth, &child)
            : fastjson_merge_patch_to_zval(wctx, member->value,
                                           patch_member->value, child_depth,
                                           &child);
        if (UNEXPECTED(!child_ok)) {
            zval_ptr_dtor(&child);
            ok = false;
            break;
        }
        if (ht == NULL) {
            ht = zend_new_array(8);
            zend_hash_real_init_mixed(ht);
            Z_OBJ_P(out)->properties = ht;
        }
        if (UNEXPECTED(!fj_merge_emit_key_val(wctx, ht, obj_share,
                                              member->key, &child))) {
            ok = false;
            break;
        }
    }

    for (size_t i = 0; ok && i < patch_index.count; i++) {
        if (UNEXPECTED(fastjson_oom_pending())) {
            ok = false;
            break;
        }
        fj_merge_member *member = &patch_index.members[i];
        if (member->emitted || yyjson_is_null(member->value)) {
            continue;
        }
        zval child;
        if (UNEXPECTED(!fastjson_merge_patch_to_zval(wctx, NULL, member->value,
                                                     child_depth, &child))) {
            zval_ptr_dtor(&child);
            ok = false;
            break;
        }
        if (ht == NULL) {
            ht = zend_new_array(8);
            zend_hash_real_init_mixed(ht);
            Z_OBJ_P(out)->properties = ht;
        }
        if (UNEXPECTED(!fj_merge_emit_key_val(wctx, ht, obj_share,
                                              member->key, &child))) {
            ok = false;
            break;
        }
    }

    fj_merge_index_destroy(&patch_index);
    fj_merge_index_destroy(&orig_index);
    return ok;
}

static bool fastjson_merge_patch_linear_to_zval(const fj_walk_ctx *wctx,
                                                yyjson_val *orig,
                                                yyjson_val *patch,
                                                zend_long remaining_depth,
                                                zval *out)
{
    if (fj_merge_has_duplicate_keys(orig)
            || fj_merge_has_duplicate_keys(patch)) {
        return fastjson_merge_patch_indexed_to_zval(
            wctx, orig, patch, remaining_depth, out);
    }

    size_t orig_size = yyjson_is_obj(orig) ? yyjson_obj_size(orig) : 0;
    size_t patch_size = yyjson_obj_size(patch);
    size_t hint = orig_size + patch_size;
    HashTable *ht = fj_merge_alloc_container(wctx->assoc, hint, out);
    fj_key_cache *obj_share = (wctx->share != NULL && !wctx->share->off)
        ? wctx->share : NULL;
    zend_long child_depth = remaining_depth - 1;

    if (yyjson_is_obj(orig)) {
        yyjson_val *key;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(orig, &iter);
        while ((key = yyjson_obj_iter_next(&iter))) {
            if (UNEXPECTED(fastjson_oom_pending())) {
                return false;
            }
            yyjson_val *orig_value = yyjson_obj_iter_get_val(key);
            yyjson_val *patch_value = yyjson_obj_getn(
                patch, yyjson_get_str(key), yyjson_get_len(key));
            if (patch_value != NULL && yyjson_is_null(patch_value)) {
                continue;
            }
            zval child;
            bool child_ok = patch_value == NULL
                ? fj_merge_walk_val(wctx, orig_value, child_depth, &child)
                : fastjson_merge_patch_to_zval(wctx, orig_value, patch_value,
                                               child_depth, &child);
            if (UNEXPECTED(!child_ok)) {
                zval_ptr_dtor(&child);
                return false;
            }
            if (ht == NULL) {
                ht = zend_new_array(8);
                zend_hash_real_init_mixed(ht);
                Z_OBJ_P(out)->properties = ht;
            }
            if (UNEXPECTED(!fj_merge_emit_key_val(wctx, ht, obj_share,
                                                  key, &child))) {
                return false;
            }
        }
    }

    yyjson_val *key;
    yyjson_obj_iter iter;
    yyjson_obj_iter_init(patch, &iter);
    while ((key = yyjson_obj_iter_next(&iter))) {
        if (UNEXPECTED(fastjson_oom_pending())) {
            return false;
        }
        yyjson_val *patch_value = yyjson_obj_iter_get_val(key);
        if (yyjson_is_null(patch_value)) {
            continue;
        }
        if (yyjson_is_obj(orig)
                && yyjson_obj_getn(orig, yyjson_get_str(key),
                                   yyjson_get_len(key)) != NULL) {
            continue;
        }
        zval child;
        if (UNEXPECTED(!fastjson_merge_patch_to_zval(wctx, NULL, patch_value,
                                                     child_depth, &child))) {
            zval_ptr_dtor(&child);
            return false;
        }
        if (ht == NULL) {
            ht = zend_new_array(8);
            zend_hash_real_init_mixed(ht);
            Z_OBJ_P(out)->properties = ht;
        }
        if (UNEXPECTED(!fj_merge_emit_key_val(wctx, ht, obj_share,
                                              key, &child))) {
            return false;
        }
    }
    return true;
}

/* Direct RFC 7396 merge patch into PHP zvals without allocating a mutable
 * yyjson document or pre-scanning subtree depth. */
static bool fastjson_merge_patch_to_zval(const fj_walk_ctx *wctx,
                                         yyjson_val *orig,
                                         yyjson_val *patch,
                                         zend_long remaining_depth,
                                         zval *out)
{
    if (UNEXPECTED(remaining_depth <= 0
            || zend_call_stack_overflowed(EG(stack_limit)))) {
        fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                "Maximum stack depth exceeded");
        ZVAL_NULL(out);
        return false;
    }
    if (!yyjson_is_obj(patch)) {
        return fj_merge_walk_val(wctx, patch, remaining_depth, out);
    }
    if (UNEXPECTED(remaining_depth <= 1)) {
        fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                "Maximum stack depth exceeded");
        ZVAL_NULL(out);
        return false;
    }

    size_t patch_size = yyjson_obj_size(patch);
    size_t orig_size = yyjson_is_obj(orig) ? yyjson_obj_size(orig) : 0;
    if (patch_size <= 16 && orig_size <= 16) {
        return fastjson_merge_patch_linear_to_zval(
            wctx, orig, patch, remaining_depth, out);
    }
    return fastjson_merge_patch_indexed_to_zval(
        wctx, orig, patch, remaining_depth, out);
}

/* True if strict JSON `s` definitely does not start with an object '{'. */
static bool fj_patch_is_non_object_strict(const char *s, size_t len,
                                          zend_long flags)
{
    if (flags & FASTJSON_DECODE_RELAXED) {
        return false;
    }
    size_t i = 0;
    while (i < len && (s[i] == ' ' || s[i] == '\t'
                       || s[i] == '\n' || s[i] == '\r')) {
        i++;
    }
    return i < len && s[i] != '{';
}

static void fastjson_merge_patch_impl(const char *target, size_t target_len,
                                      const char *patch, size_t patch_len,
                                      bool assoc, bool assoc_is_null,
                                      zend_long depth, zend_long flags,
                                      zval *return_value)
{
    bool throw_mode;
    fastjson_error_state saved_err;
    if (!fastjson_entry_prologue(flags, FASTJSON_DECODE_THROW_ON_ERROR,
                                 depth, 4, &throw_mode, &saved_err)) {
        RETURN_THROWS();
    }

    bool use_assoc = assoc_is_null
        ? ((flags & FASTJSON_DECODE_OBJECT_AS_ARRAY) != 0)
        : (bool)assoc;

    /* When $patch is a non-object, RFC 7396 replaces $target wholesale. Validate
     * $target first with VALIDATE_ONLY so $target-first parse-error precedence
     * holds without building $target's value tree. */
    if (fj_patch_is_non_object_strict(patch, patch_len, flags)) {
        fastjson_small_alc tal_small;
        fastjson_small_alc_init(&tal_small);
        yyjson_doc *tdoc = fastjson_read_doc_with_alc(
            target, target_len, flags, YYJSON_READ_VALIDATE_ONLY, &tal_small.alc,
            throw_mode, &saved_err);
        if (tdoc == NULL) {
            if (throw_mode) {
                RETURN_THROWS();
            }
            RETURN_NULL();
        }
        yyjson_doc_free(tdoc);

        fastjson_small_alc pal_small;
        const yyjson_alc *palc = patch_len <= FASTJSON_SMALL_ALC_THRESHOLD
            ? (fastjson_small_alc_init(&pal_small), &pal_small.alc)
            : &fastjson_php_alc;
        yyjson_doc *pdoc = fastjson_read_doc_with_alc(
            patch, patch_len, flags, 0, palc, throw_mode, &saved_err);
        if (pdoc == NULL) {
            if (throw_mode) {
                RETURN_THROWS();
            }
            RETURN_NULL();
        }
        fastjson_finish_decode(pdoc, use_assoc, depth, flags, throw_mode,
                               &saved_err, return_value);
        return;
    }

    fastjson_small_alc tal_small, pal_small;
    const yyjson_alc *talc = target_len <= FASTJSON_SMALL_ALC_THRESHOLD
        ? (fastjson_small_alc_init(&tal_small), &tal_small.alc)
        : &fastjson_php_alc;
    yyjson_doc *tdoc = fastjson_read_doc_with_alc(
        target, target_len, flags, 0, talc, throw_mode, &saved_err);
    if (tdoc == NULL) {
        if (throw_mode) {
            RETURN_THROWS();
        }
        RETURN_NULL();
    }
    const yyjson_alc *palc = patch_len <= FASTJSON_SMALL_ALC_THRESHOLD
        ? (fastjson_small_alc_init(&pal_small), &pal_small.alc)
        : &fastjson_php_alc;
    yyjson_doc *pdoc = fastjson_read_doc_with_alc(
        patch, patch_len, flags, 0, palc, throw_mode, &saved_err);
    if (pdoc == NULL) {
        yyjson_doc_free(tdoc);
        if (throw_mode) {
            RETURN_THROWS();
        }
        RETURN_NULL();
    }

    zend_long walk_depth = fastjson_effective_walk_depth(depth);
    fj_key_cache *cache = fj_key_cache_acquire(target_len + patch_len);
    fj_walk_ctx wctx = { cache, flags, use_assoc };
    bool walk_ok = fastjson_merge_patch_to_zval(
        &wctx, yyjson_doc_get_root(tdoc), yyjson_doc_get_root(pdoc),
        walk_depth, return_value);
    fj_key_cache_release(cache);
    yyjson_doc_free(tdoc);
    yyjson_doc_free(pdoc);

    if (UNEXPECTED(!walk_ok)) {
        if (!Z_ISUNDEF_P(return_value)) {
            zval_ptr_dtor(return_value);
            ZVAL_NULL(return_value);
        }
        if (throw_mode) {
            fastjson_throw_current_error("fastjson_merge_patch failed",
                                         &saved_err);
            RETURN_THROWS();
        }
        if (FASTJSON_G(last_err_code) == FASTJSON_ERROR_NONE) {
            fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                    "fastjson_merge_patch failed");
        }
        RETURN_NULL();
    }
    if (!throw_mode) {
        fastjson_clear_error();
    }
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

    fastjson_merge_patch_impl(target, target_len, patch, patch_len,
                              assoc, assoc_is_null, depth, flags, return_value);
}

#if PHP_VERSION_ID >= 80400
ZEND_FRAMELESS_FUNCTION(fastjson_merge_patch, 2)
{
    zval target_tmp, patch_tmp;
    zend_string *target, *patch;

    Z_FLF_PARAM_STR(1, target, target_tmp);
    Z_FLF_PARAM_STR(2, patch, patch_tmp);
    fastjson_merge_patch_impl(ZSTR_VAL(target), ZSTR_LEN(target),
                              ZSTR_VAL(patch), ZSTR_LEN(patch),
                              false, true, 512, 0, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, target_tmp);
    Z_FLF_PARAM_FREE_STR(2, patch_tmp);
}

ZEND_FRAMELESS_FUNCTION(fastjson_merge_patch, 3)
{
    zval target_tmp, patch_tmp;
    zend_string *target, *patch;
    bool assoc = false;
    bool assoc_is_null = true;

    Z_FLF_PARAM_STR(1, target, target_tmp);
    Z_FLF_PARAM_STR(2, patch, patch_tmp);
    if (UNEXPECTED(!zend_parse_arg_bool_ex(arg3, &assoc, &assoc_is_null,
            /* check_null */ true, 3, /* frameless */ true))) {
        zend_wrong_parameter_type_error(3, Z_EXPECTED_BOOL_OR_NULL, arg3);
        goto flf_clean;
    }
    fastjson_merge_patch_impl(ZSTR_VAL(target), ZSTR_LEN(target),
                              ZSTR_VAL(patch), ZSTR_LEN(patch),
                              assoc, assoc_is_null, 512, 0, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, target_tmp);
    Z_FLF_PARAM_FREE_STR(2, patch_tmp);
}
#endif

static void fastjson_pointer_exists_impl(const char *json, size_t json_len,
                                         const char *pointer,
                                         size_t pointer_len, zend_long flags,
                                         zval *return_value)
{
    bool throw_mode = (flags & FASTJSON_DECODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_throw_mode_init(throw_mode, &saved_err);

    yyjson_read_flag extra_flags = pointer_len == 0
        ? YYJSON_READ_VALIDATE_ONLY : 0;
    fastjson_small_alc small_alc;
    const yyjson_alc *alc = (pointer_len == 0
            || json_len <= FASTJSON_SMALL_ALC_THRESHOLD)
        ? (fastjson_small_alc_init(&small_alc), &small_alc.alc)
        : &fastjson_php_alc;

    yyjson_doc *doc = fastjson_read_doc_with_alc(
        json, json_len, flags, extra_flags, alc, throw_mode, &saved_err);
    if (UNEXPECTED(doc == NULL)) {
        if (throw_mode) {
            RETURN_THROWS();
        }
        RETURN_FALSE;
    }

    if (pointer_len == 0) {
        yyjson_doc_free(doc);
        RETURN_TRUE;
    }

    yyjson_val *target;
    fj_splice_status pointer_status = fastjson_pointer_resolve_limited(
        yyjson_doc_get_root(doc), pointer, pointer_len, 0, flags, &target);
    if (pointer_status == FJ_SPLICE_AMBIGUOUS
            || pointer_status == FJ_SPLICE_DEPTH_FAIL) {
        yyjson_doc_free(doc);
        if (fj_splice_dispatch_error(pointer_status, throw_mode,
                                     &saved_err)) {
            RETURN_THROWS();
        }
        RETURN_FALSE;
    }
    bool exists = target != NULL;
    yyjson_doc_free(doc);
    if (!exists) {
        fastjson_clear_error();
    }
    RETURN_BOOL(exists);
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

    fastjson_pointer_exists_impl(json, json_len, pointer, pointer_len,
                                 flags, return_value);
}

#if PHP_VERSION_ID >= 80400
ZEND_FRAMELESS_FUNCTION(fastjson_pointer_exists, 2)
{
    zval json_tmp, ptr_tmp;
    zend_string *json, *ptr;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    Z_FLF_PARAM_STR(2, ptr, ptr_tmp);
    fastjson_pointer_exists_impl(ZSTR_VAL(json), ZSTR_LEN(json),
                                 ZSTR_VAL(ptr), ZSTR_LEN(ptr),
                                 0, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
    Z_FLF_PARAM_FREE_STR(2, ptr_tmp);
}

ZEND_FRAMELESS_FUNCTION(fastjson_pointer_exists, 3)
{
    zval json_tmp, ptr_tmp;
    zend_string *json, *ptr;
    zend_long flags;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    Z_FLF_PARAM_STR(2, ptr, ptr_tmp);
    Z_FLF_PARAM_LONG(3, flags);
    fastjson_pointer_exists_impl(ZSTR_VAL(json), ZSTR_LEN(json),
                                 ZSTR_VAL(ptr), ZSTR_LEN(ptr),
                                 flags, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
    Z_FLF_PARAM_FREE_STR(2, ptr_tmp);
}
#endif

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

    bool throw_mode;
    fastjson_error_state saved_err;
    if (!fastjson_entry_prologue(flags, FASTJSON_DECODE_THROW_ON_ERROR,
                                 depth, 4, &throw_mode, &saved_err)) {
        RETURN_THROWS();
    }
    size_t output_depth = (size_t)fastjson_effective_walk_depth(depth);

    /* Root replacement (pointer == "") discards the parsed base tree, so
     * VALIDATE_ONLY checks $json syntax without allocating a value tree. */
    yyjson_read_flag extra_read_flags = pointer_len == 0
        ? YYJSON_READ_VALIDATE_ONLY
        : YYJSON_READ_NUMBER_AS_RAW;
    fastjson_small_alc small_alc;
    const yyjson_alc *alc = (pointer_len == 0
            || json_len <= FASTJSON_SMALL_ALC_THRESHOLD)
        ? (fastjson_small_alc_init(&small_alc), &small_alc.alc)
        : &fastjson_php_alc;

    yyjson_doc *idoc = fastjson_read_doc_with_alc(
        json, json_len, flags & FASTJSON_DECODE_RELAXED,
        extra_read_flags, alc, throw_mode, &saved_err);
    if (UNEXPECTED(idoc == NULL)) {
        if (throw_mode) {
            RETURN_THROWS();
        }
        RETURN_FALSE;
    }

    fastjson_pointer_plan plan;
    fj_splice_status splice_status = FJ_SPLICE_OK;
    if (pointer_len == 0) {
        plan.segs = NULL;
        plan.storage = NULL;
        plan.nsegs = 0;
    } else if (!fastjson_pointer_plan_init(yyjson_doc_get_root(idoc), pointer,
                                           pointer_len, output_depth, &plan,
                                           &splice_status)) {
        yyjson_doc_free(idoc);
        goto pointer_set_failed;
    }

    zend_long value_flags = flags & FASTJSON_POINTER_VALUE_ENCODE_FLAGS;

    /* Subtract path containers from the replacement budget; scalars allow zero. */
    zend_long repl_depth = (zend_long)output_depth - (zend_long)plan.nsegs - 1;
    if (repl_depth < 0) {
        repl_depth = 0;
    }

    fastjson_pointer_repl repl;
    fastjson_error_state replacement_err;
    bool replacement_ok = fastjson_pointer_build_replacement(
        value, value_flags, repl_depth, &repl, &replacement_err);
    if (!throw_mode) {
        fastjson_restore_error_state(&replacement_err);
    }
    if (!replacement_ok) {
        fastjson_pointer_plan_destroy(&plan);
        yyjson_doc_free(idoc);
        if (EG(exception)) {
            RETURN_THROWS();
        }
        if (throw_mode) {
            fastjson_throw_error(replacement_err.code, replacement_err.msg,
                                 "fastjson_pointer_set encode failed",
                                 &saved_err);
            RETURN_THROWS();
        }
        RETURN_FALSE;
    }

    zend_string *out = fastjson_imut_pointer_set_write(
        pointer_len == 0 ? NULL : yyjson_doc_get_root(idoc),
        &plan, &repl, flags, output_depth, json_len, &splice_status);

    fastjson_pointer_plan_destroy(&plan);
    if (repl.owned_str != NULL) {
        efree(repl.owned_str);
        repl.owned_str = NULL;
    }
    if (repl.encoded != NULL) {
        zend_string_release(repl.encoded);
        repl.encoded = NULL;
    }
    yyjson_doc_free(idoc);

    if (out == NULL) {
pointer_set_failed:
        if (fj_splice_dispatch_error(splice_status, throw_mode, &saved_err)) {
            RETURN_THROWS();
        }
        RETURN_FALSE;
    }

    if (throw_mode) {
        fastjson_restore_error_state(
            replacement_err.code != FASTJSON_ERROR_NONE
                ? &replacement_err : &saved_err);
    }
    RETVAL_STR(out);
}
