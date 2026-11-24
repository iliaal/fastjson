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

/* Include through php_fastjson.h for the required PHP and yyjson types. */

#ifndef PHP_FASTJSON_INTERNAL_H
#define PHP_FASTJSON_INTERNAL_H

#include "php_streams.h"
#include "zend_bitset.h"

static zend_always_inline bool fastjson_depth_fits_int(zend_long depth)
{
#ifdef ZEND_IN_RANGE_OF
    return ZEND_IN_RANGE_OF(depth, int);
#else
    return !ZEND_LONG_EXCEEDS_INT(depth);
#endif
}

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

#define FASTJSON_HAS_UTF8_HANDLING_FLAG(flags) \
    (((flags) & (FASTJSON_INVALID_UTF8_IGNORE \
                 | FASTJSON_INVALID_UTF8_SUBSTITUTE)) != 0)

/* Strings from 8 KiB take the sized writer when their 6x reserve would
 * grow the buffer. Below 8 MiB it copies clean ASCII at its exact size,
 * writes a string whose escapes sit in the last eighth as a copied prefix
 * plus a 6x tail, and writes any other string in 2 KiB chunks, which
 * reserve the input length plus 12 KiB instead of 6x (a 137 KiB PHP
 * source string peaked at 828 KiB under the 6x reserve). From 8 MiB it
 * copies clean ASCII in one fused scan and measures the exact output of
 * anything else first. Shorter strings, and reserves that
 * already fit, keep yyjson's one-pass writer, which is faster and whose
 * headroom costs at most 48 KiB. From 256 KiB the sized writer is always
 * used: it measured -35%/-31% on x86_64 and +8% on aarch64 versus 6x at
 * that size; re-measure both before moving it. */
#define FASTJSON_EXACT_STRING_THRESHOLD (8 * 1024)
#define FASTJSON_EXACT_STRING_ALWAYS (256 * 1024)

static zend_always_inline bool fastjson_string_wants_sized_writer(
    const smart_str *buf, size_t len)
{
    if (len < FASTJSON_EXACT_STRING_THRESHOLD) {
        return false;
    }
    if (len >= FASTJSON_EXACT_STRING_ALWAYS) {
        return true;
    }
    size_t used = buf->s ? ZSTR_LEN(buf->s) : 0;
    /* smart_str_alloc() grows once used + add reaches a, so equal is short. */
    return buf->a < used || buf->a - used <= len * 6 + 2;
}
/* Non-ASCII preflight measured +75%/+160% on x86_64/aarch64 at 1 MiB.
 * Delay it until the 6x reservation reaches 48 MiB against a 128M memory_limit.
 * Clean ASCII bypasses preflight via the fused scan-and-copy path. */
#define FASTJSON_EXACT_NONASCII_THRESHOLD (8 * 1024 * 1024)
#define FASTJSON_ENCODE_HEX_MASK (FASTJSON_ENCODE_HEX_TAG \
    | FASTJSON_ENCODE_HEX_AMP | FASTJSON_ENCODE_HEX_APOS \
    | FASTJSON_ENCODE_HEX_QUOT)
/* yyjson_write_number requires at least 21 bytes for integers, 40 for reals. */
#define FASTJSON_NUM_INT_WORST 24
#define FASTJSON_NUM_REAL_WORST 40

/* Initial smart_str request stays within Zend MM's 256-byte small slot
 * (SMART_STR_START_LEN = 256 - 24 - 1 = 231 B); 256 would jump directly to a 4 KiB page. */
#define FASTJSON_SMART_STR_INIT_CAP 224

/* 33-byte static "\n" + 32 spaces table collapses level <= 8 to one append. */
static zend_always_inline void fastjson_append_newline_indent(
    smart_str *buf, int level)
{
    static const char nl_spaces[] = "\n                                ";
    if (EXPECTED(level <= 8)) {
        smart_str_appendl(buf, nl_spaces, 1 + (size_t)level * 4);
        return;
    }
    if (EXPECTED((size_t)level <= (ZSTR_MAX_LEN - 1) / 4)) {
        size_t spaces = (size_t)level * 4;
        smart_str_alloc(buf, spaces + 1, 0);
        char *p = ZSTR_VAL(buf->s) + ZSTR_LEN(buf->s);
        *p++ = '\n';
        memset(p, ' ', spaces);
        ZSTR_LEN(buf->s) += spaces + 1;
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

#define FASTJSON_POINTER_INLINE_SEGS    16
#define FASTJSON_POINTER_INLINE_STORAGE 256

typedef struct {
    fj_ptr_seg *segs;
    char *storage;
    size_t nsegs;
    fj_ptr_seg inline_segs[FASTJSON_POINTER_INLINE_SEGS];
    char inline_storage[FASTJSON_POINTER_INLINE_STORAGE];
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

zend_string *fastjson_imut_pointer_set_write(yyjson_val *root,
                                             const fastjson_pointer_plan *plan,
                                             const fastjson_pointer_repl *replacement,
                                             zend_long flags,
                                             size_t depth_limit,
                                             size_t hint_len,
                                             fj_splice_status *status);

#if defined(__SSE2__)
# include <emmintrin.h>
#elif defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
# include <arm_neon.h>
#endif

extern const uint8_t fastjson_str_esc_tbl[256];
extern const char fastjson_digit_pairs[201];

/* Copy clean printable ASCII (no control, '"', '\\', >=0x80, or '/' when
 * escape_slashes is set) from s[0..len) into dst[0..len), returning the index
 * of the first byte needing escape (or len if all clean). */
static zend_always_inline size_t fastjson_copy_clean_ascii(
    char *dst, const char *s, size_t len, bool escape_slashes)
{
    size_t i = 0;
#if defined(__SSE2__)
    if (len >= 16) {
        const __m128i v_min = _mm_set1_epi8(0x20);
        const __m128i v_quot = _mm_set1_epi8('"');
        const __m128i v_bsl = _mm_set1_epi8('\\');
        const __m128i v_fsl = _mm_set1_epi8('/');
        for (; i + 16 <= len; i += 16) {
            __m128i v = _mm_loadu_si128((const __m128i *)(s + i));
            /* Signed < 0x20 catches both [0x00..0x1F] and [0x80..0xFF]. */
            __m128i bad = _mm_cmplt_epi8(v, v_min);
            bad = _mm_or_si128(bad, _mm_cmpeq_epi8(v, v_quot));
            bad = _mm_or_si128(bad, _mm_cmpeq_epi8(v, v_bsl));
            if (escape_slashes) {
                bad = _mm_or_si128(bad, _mm_cmpeq_epi8(v, v_fsl));
            }
            int mask = _mm_movemask_epi8(bad);
            if (UNEXPECTED(mask != 0)) {
                size_t clean = (size_t)zend_ulong_ntz((zend_ulong)(unsigned int)mask);
                for (size_t k = 0; k < clean; k++) {
                    dst[i + k] = s[i + k];
                }
                return i + clean;
            }
            _mm_storeu_si128((__m128i *)(dst + i), v);
        }
    }
#elif defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
    if (len >= 16) {
        const uint8x16_t v_sub = vdupq_n_u8(0x20);
        const uint8x16_t v_lim = vdupq_n_u8(0x5F);
        const uint8x16_t v_quot = vdupq_n_u8('"');
        const uint8x16_t v_bsl = vdupq_n_u8('\\');
        const uint8x16_t v_fsl = vdupq_n_u8('/');
        for (; i + 16 <= len; i += 16) {
            uint8x16_t v = vld1q_u8((const uint8_t *)(s + i));
            /* Unsigned (v - 0x20) > 0x5F catches both <0x20 and >=0x80. */
            uint8x16_t bad = vcgtq_u8(vsubq_u8(v, v_sub), v_lim);
            bad = vorrq_u8(bad, vceqq_u8(v, v_quot));
            bad = vorrq_u8(bad, vceqq_u8(v, v_bsl));
            if (escape_slashes) {
                bad = vorrq_u8(bad, vceqq_u8(v, v_fsl));
            }
            if (UNEXPECTED(vmaxvq_u8(bad) != 0)) {
                break;
            }
            vst1q_u8((uint8_t *)(dst + i), v);
        }
    }
#endif
    uint8_t esc_mask = escape_slashes ? 3u : 1u;
    for (; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (UNEXPECTED((fastjson_str_esc_tbl[c] & esc_mask) != 0)) {
            return i;
        }
        dst[i] = (char)c;
    }
    return len;
}

/* Out-of-line decimal writer for uint64_t >= 10000 (defined in fastjson.c). */
char *fastjson_write_u64_large(char *dst, uint64_t u);

/* Signed 64-bit decimal writer using division-free reciprocal multiply and a
 * 2-digit table. Caller must ensure >= 24 writable bytes at dst. */
static zend_always_inline char *fastjson_write_i64(char *dst, int64_t val)
{
    uint64_t u;
    if (val < 0) {
        *dst++ = '-';
        u = (uint64_t)(-(val + 1)) + 1u;
    } else {
        u = (uint64_t)val;
    }
    if (EXPECTED(u < 100u)) {
        uint32_t lz = u < 10u;
        memcpy(dst, &fastjson_digit_pairs[u * 2u + lz], 2);
        return dst + 2u - lz;
    }
    if (EXPECTED(u < 10000u)) {
        uint32_t v = (uint32_t)u;
        uint32_t aa = (v * 5243u) >> 19;
        uint32_t bb = v - aa * 100u;
        uint32_t lz = aa < 10u;
        memcpy(dst, &fastjson_digit_pairs[aa * 2u + lz], 2);
        dst += 2u - lz;
        memcpy(dst, &fastjson_digit_pairs[bb * 2u], 2);
        return dst + 2;
    }
    return fastjson_write_u64_large(dst, u);
}

/* Persistent per-thread decode key cache cleanup hooks (fastjson_decode.c). */
void fastjson_key_cache_rinit(void);
void fastjson_key_cache_rshutdown(void);
void fastjson_key_cache_destroy(void *cache);

/* Shared PHP_FUNCTION entry prologue for the decode family, defined in
 * fastjson.c: snapshot the throw-mode error state, then validate $depth
 * exactly as ext/json does. Returns true on success; on $depth failure
 * raises ValueError and returns false, in which case the caller must
 * RETURN_THROWS(). */
bool fastjson_entry_prologue(zend_long flags, zend_long throw_bit,
                             zend_long depth, int depth_argno,
                             bool *throw_mode_out,
                             fastjson_error_state *saved_out);

/* Read a JSON document with a caller-chosen yyjson_alc (used with
 * fastjson_small_alc for short inputs). */
yyjson_doc *fastjson_read_doc_alc(const char *json, size_t json_len,
                                  zend_long flags,
                                  yyjson_read_flag extra_yflags,
                                  const yyjson_alc *alc,
                                  yyjson_read_err *err);

#endif /* PHP_FASTJSON_INTERNAL_H */
