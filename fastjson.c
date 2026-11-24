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

#include <limits.h>

#include "php.h"
#include "php_ini.h"
#include "Zend/zend_exceptions.h"
#include "ext/standard/info.h"
#include "main/php_main.h"
#include "php_streams.h"
#include "ext/standard/file.h"
#include "php_fastjson.h"
#include "fastjson_arginfo.h"
#include "fastjson_alloc.h"
#include "yyjson.h"

ZEND_DECLARE_MODULE_GLOBALS(fastjson)

zend_class_entry *fastjson_json_exception_ce = NULL;
zend_class_entry *fastjson_json_serializable_ce = NULL;
zend_string *fastjson_jsonserialize_str = NULL;

/* Bit 0: always needs JSON escaping (0x00..0x1F, '"', '\\', 0x80..0xFF).
 * Bit 1: '/' (needs escaping when YYJSON_WRITE_ESCAPE_SLASHES is set). */
const uint8_t fastjson_str_esc_tbl[256] = {
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,2, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
    1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1, 1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,
};

const char fastjson_digit_pairs[201] =
    "0001020304050607080910111213141516171819"
    "2021222324252627282930313233343536373839"
    "4041424344454647484950515253545556575859"
    "6061626364656667686970717273747576777879"
    "8081828384858687888990919293949596979899";

static zend_always_inline char *fj_write_u32_len_4(char *dst, uint32_t v)
{
    uint32_t aa = (v * 5243u) >> 19;
    uint32_t bb = v - aa * 100u;
    memcpy(dst, &fastjson_digit_pairs[aa * 2u], 2);
    memcpy(dst + 2, &fastjson_digit_pairs[bb * 2u], 2);
    return dst + 4;
}

static zend_always_inline char *fj_write_u32_len_8(char *dst, uint32_t v)
{
    uint32_t aabb = (uint32_t)(((uint64_t)v * 109951163u) >> 40);
    uint32_t ccdd = v - aabb * 10000u;
    dst = fj_write_u32_len_4(dst, aabb);
    return fj_write_u32_len_4(dst, ccdd);
}

static zend_always_inline char *fj_write_u32_len_1_to_8(char *dst, uint32_t v)
{
    if (v < 100u) {
        uint32_t lz = v < 10u;
        memcpy(dst, &fastjson_digit_pairs[v * 2u + lz], 2);
        return dst + 2u - lz;
    }
    if (v < 10000u) {
        uint32_t aa = (v * 5243u) >> 19;
        uint32_t bb = v - aa * 100u;
        uint32_t lz = aa < 10u;
        memcpy(dst, &fastjson_digit_pairs[aa * 2u + lz], 2);
        dst += 2u - lz;
        memcpy(dst, &fastjson_digit_pairs[bb * 2u], 2);
        return dst + 2;
    }
    if (v < 1000000u) {
        uint32_t aa = (uint32_t)(((uint64_t)v * 429497u) >> 32);
        uint32_t bbcc = v - aa * 10000u;
        uint32_t lz = aa < 10u;
        memcpy(dst, &fastjson_digit_pairs[aa * 2u + lz], 2);
        dst += 2u - lz;
        return fj_write_u32_len_4(dst, bbcc);
    }
    uint32_t aabb = (uint32_t)(((uint64_t)v * 109951163u) >> 40);
    uint32_t ccdd = v - aabb * 10000u;
    uint32_t aa = (aabb * 5243u) >> 19;
    uint32_t bb = aabb - aa * 100u;
    uint32_t lz = aa < 10u;
    memcpy(dst, &fastjson_digit_pairs[aa * 2u + lz], 2);
    dst += 2u - lz;
    memcpy(dst, &fastjson_digit_pairs[bb * 2u], 2);
    return fj_write_u32_len_4(dst + 2, ccdd);
}

char *fastjson_write_u64_large(char *dst, uint64_t u)
{
    if (u < 100000000u) {
        return fj_write_u32_len_1_to_8(dst, (uint32_t)u);
    }
    if (u < UINT64_C(10000000000000000)) {
        uint64_t hgh = u / 100000000u;
        uint32_t low = (uint32_t)(u - hgh * 100000000u);
        dst = fj_write_u32_len_1_to_8(dst, (uint32_t)hgh);
        return fj_write_u32_len_8(dst, low);
    }
    uint64_t tmp = u / 100000000u;
    uint32_t low = (uint32_t)(u - tmp * 100000000u);
    uint32_t hgh = (uint32_t)(tmp / 10000u);
    uint32_t mid = (uint32_t)(tmp - (uint64_t)hgh * 10000u);
    dst = fj_write_u32_len_1_to_8(dst, hgh);
    dst = fj_write_u32_len_4(dst, mid);
    return fj_write_u32_len_8(dst, low);
}

static bool fastjson_locate_ascii_pos(const char *json, size_t pos,
                                      size_t *line, size_t *col)
{
    size_t start = 0;
    if (pos >= 3
            && (unsigned char)json[0] == 0xEF
            && (unsigned char)json[1] == 0xBB
            && (unsigned char)json[2] == 0xBF) {
        start = 3;
    }

    size_t line_count = 0;
    size_t line_start = start;
    size_t i = start;
    const uint64_t ones = UINT64_C(0x0101010101010101);
    const uint64_t highs = UINT64_C(0x8080808080808080);
    const uint64_t newlines = ones * (unsigned char)'\n';
    for (; i + sizeof(uint64_t) <= pos; i += sizeof(uint64_t)) {
        uint64_t word;
        memcpy(&word, json + i, sizeof(word));
        if (word & highs) {
            return false;
        }
        uint64_t x = word ^ newlines;
        if (((x - ones) & ~x & highs) != 0) {
            for (size_t j = 0; j < sizeof(uint64_t); j++) {
                if (json[i + j] == '\n') {
                    line_count++;
                    line_start = i + j + 1;
                }
            }
        }
    }
    for (; i < pos; i++) {
        unsigned char c = (unsigned char)json[i];
        if (c >= 0x80) {
            return false;
        }
        if (c == '\n') {
            line_count++;
            line_start = i + 1;
        }
    }

    *line = line_count + 1;
    *col = pos - line_start + 1;
    return true;
}

zend_long fastjson_translate_read_code(yyjson_read_code yy)
{
    if (yy == YYJSON_READ_SUCCESS) {
        return FASTJSON_ERROR_NONE;
    }
    if (yy == YYJSON_READ_ERROR_INVALID_STRING) {
        /* yyjson surfaces invalid UTF-8 inside a JSON string under this
         * code; ext/json reports the same condition as JSON_ERROR_UTF8. */
        return FASTJSON_ERROR_UTF8;
    }
    return FASTJSON_ERROR_SYNTAX;
}

void fastjson_set_read_error(const char *json, size_t json_len,
                             const yyjson_read_err *err)
{
    FASTJSON_G(last_err_code) = fastjson_translate_read_code(err->code);
    FASTJSON_G(last_err_msg) = err->msg;
    FASTJSON_G(last_err_pos) = -1;
    FASTJSON_G(last_err_line) = 0;
    FASTJSON_G(last_err_col) = 0;
    if (err->code != YYJSON_READ_SUCCESS && err->pos <= json_len) {
        size_t line = 0, col = 0;
        FASTJSON_G(last_err_pos) = (zend_long)err->pos;
        if (fastjson_locate_ascii_pos(json, err->pos, &line, &col)
                || yyjson_locate_pos(json, json_len, err->pos,
                                     &line, &col, NULL)) {
            FASTJSON_G(last_err_line) = (zend_long)line;
            FASTJSON_G(last_err_col) = (zend_long)col;
        }
    }
}

static zend_class_entry *fastjson_exception_ce(void)
{
    return fastjson_json_exception_ce
        ? fastjson_json_exception_ce : zend_ce_exception;
}

void fastjson_throw_error(zend_long code, const char *msg,
                          const char *fallback_msg,
                          const fastjson_error_state *saved_err)
{
    if (msg == NULL) {
        msg = fallback_msg ? fallback_msg : "fastjson error";
    }
    zend_throw_exception(fastjson_exception_ce(), msg, code);
    if (saved_err != NULL) {
        fastjson_restore_error_state(saved_err);
    }
}

void fastjson_throw_current_error(const char *fallback_msg,
                                  const fastjson_error_state *saved_err)
{
    fastjson_throw_error(FASTJSON_G(last_err_code), FASTJSON_G(last_err_msg),
                         fallback_msg, saved_err);
}

void fastjson_throw_read_error(const yyjson_read_err *err,
                               const fastjson_error_state *saved_err)
{
    fastjson_throw_error(fastjson_translate_read_code(err->code), err->msg,
                         "JSON parse error", saved_err);
}

bool fastjson_entry_prologue(zend_long flags, zend_long throw_bit,
                             zend_long depth, int depth_argno,
                             bool *throw_mode_out,
                             fastjson_error_state *saved_out)
{
    bool throw_mode = (flags & throw_bit) != 0;
    fastjson_throw_mode_init(throw_mode, saved_out);
    if (depth <= 0) {
        zend_argument_value_error(depth_argno, "must be greater than 0");
        return false;
    }
    if (depth > INT_MAX) {
        zend_argument_value_error(depth_argno, "must be less than %d", INT_MAX);
        return false;
    }
    *throw_mode_out = throw_mode;
    return true;
}

bool fastjson_byte_is_valid_utf8_start(const char *s, size_t len, size_t pos)
{
    if (pos >= len) return false;
    unsigned char c0 = (unsigned char)s[pos];
    if (c0 < 0x80) return true;
    if (c0 < 0xC2) return false; /* 0x80-0xBF lone cont, 0xC0/0xC1 overlong */

    size_t need;
    if (c0 < 0xE0) need = 1;
    else if (c0 < 0xF0) need = 2;
    else if (c0 < 0xF5) need = 3;
    else return false; /* 0xF5-0xFF: encodes beyond U+10FFFF */

    if (pos + need >= len) return false; /* truncated */
    for (size_t i = 1; i <= need; i++) {
        if (((unsigned char)s[pos + i] & 0xC0) != 0x80) return false;
    }
    /* Range restrictions inside the otherwise-valid byte pattern. */
    unsigned char c1 = (unsigned char)s[pos + 1];
    if (need == 2) {
        if (c0 == 0xE0 && c1 < 0xA0) return false; /* overlong */
        if (c0 == 0xED && c1 >= 0xA0) return false; /* UTF-16 surrogate */
    } else if (need == 3) {
        if (c0 == 0xF0 && c1 < 0x90) return false; /* overlong */
        if (c0 == 0xF4 && c1 >= 0x90) return false; /* > U+10FFFF */
    }
    return true;
}

/* Match ext/standard/html.c's UTF-8 advance rules. */
#define FJ_UTF8_TRAIL(c) ((c) >= 0x80 && (c) <= 0xBF)
#define FJ_UTF8_LEAD(c)  ((c) < 0x80 || ((c) >= 0xC2 && (c) <= 0xF4))

/* Write sanitized UTF-8 from s[0..len) into dst (must hold len*3+1 for
 * substitute, len+1 for ignore) and return written byte count. */
static size_t fastjson_sanitize_utf8_into(const char *s, size_t len,
                                          bool substitute, bool encode_advance,
                                          char *out)
{
    char *w = out;
    size_t i = 0;
    while (i < len) {
#if defined(__SSE2__)
        while (i + 16 <= len) {
            __m128i v = _mm_loadu_si128((const __m128i *)(s + i));
            if (UNEXPECTED(_mm_movemask_epi8(v) != 0)) {
                break;
            }
            _mm_storeu_si128((__m128i *)w, v);
            w += 16;
            i += 16;
        }
#elif defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
        while (i + 16 <= len) {
            uint8x16_t v = vld1q_u8((const uint8_t *)(s + i));
            if (UNEXPECTED(vmaxvq_u8(v) >= 0x80)) {
                break;
            }
            vst1q_u8((uint8_t *)w, v);
            w += 16;
            i += 16;
        }
#else
        while (i + 8 <= len) {
            uint64_t chunk;
            memcpy(&chunk, s + i, sizeof(chunk));
            if (UNEXPECTED((chunk & UINT64_C(0x8080808080808080)) != 0)) {
                break;
            }
            memcpy(w, &chunk, sizeof(chunk));
            w += 8;
            i += 8;
        }
#endif
        if (i >= len) {
            break;
        }
        unsigned char c = (unsigned char)s[i];
        size_t adv;
        bool good;
        if (c < 0x80) {
            *w++ = (char)c;
            i += 1;
            continue;
        } else if (c < 0xC2) {
            /* Lone continuation or 0xC0/0xC1 overlong lead. */
            adv = 1; good = false;
        } else if (c < 0xE0) {
            /* 2-byte sequence. */
            if (i + 1 >= len || !FJ_UTF8_TRAIL((unsigned char)s[i + 1])) {
                adv = (i + 1 >= len || FJ_UTF8_LEAD((unsigned char)s[i + 1])) ? 1 : 2;
                good = false;
            } else {
                /* C2..DF followed by a trail byte is already shortest-form. */
                adv = 2; good = true;
            }
        } else if (c < 0xF0) {
            /* 3-byte sequence. */
            size_t avail = len - i;
            if (avail < 3
                    || !FJ_UTF8_TRAIL((unsigned char)s[i + 1])
                    || !FJ_UTF8_TRAIL((unsigned char)s[i + 2])) {
                if (avail < 2 || FJ_UTF8_LEAD((unsigned char)s[i + 1]))      adv = 1;
                else if (avail < 3 || FJ_UTF8_LEAD((unsigned char)s[i + 2])) adv = 2;
                else                                                          adv = 3;
                good = false;
            } else {
                unsigned int cp = ((c & 0x0f) << 12)
                                | (((unsigned char)s[i + 1] & 0x3f) << 6)
                                |  ((unsigned char)s[i + 2] & 0x3f);
                if (cp < 0x800)                       { adv = 3; good = false; }
                else if (cp >= 0xD800 && cp <= 0xDFFF){ adv = 3; good = false; }
                else                                   { adv = 3; good = true;  }
            }
        } else if (c < 0xF5) {
            /* 4-byte sequence. */
            size_t avail = len - i;
            if (avail < 4
                    || !FJ_UTF8_TRAIL((unsigned char)s[i + 1])
                    || !FJ_UTF8_TRAIL((unsigned char)s[i + 2])
                    || !FJ_UTF8_TRAIL((unsigned char)s[i + 3])) {
                if (avail < 2 || FJ_UTF8_LEAD((unsigned char)s[i + 1]))      adv = 1;
                else if (avail < 3 || FJ_UTF8_LEAD((unsigned char)s[i + 2])) adv = 2;
                else if (avail < 4 || FJ_UTF8_LEAD((unsigned char)s[i + 3])) adv = 3;
                else                                                          adv = 4;
                good = false;
            } else {
                unsigned int cp = ((c & 0x07) << 18)
                                | (((unsigned char)s[i + 1] & 0x3f) << 12)
                                | (((unsigned char)s[i + 2] & 0x3f) << 6)
                                |  ((unsigned char)s[i + 3] & 0x3f);
                if (cp < 0x10000 || cp > 0x10FFFF) { adv = 4; good = false; }
                else                               { adv = 4; good = true;  }
            }
        } else {
            /* 0xF5-0xFF: outside the legal UTF-8 lead range entirely. */
            adv = 1; good = false;
        }

        if (good) {
            memcpy(w, s + i, adv);
            w += adv;
        } else {
            /* Decode-side substitutes one U+FFFD per byte that breaks
             * the parse, matching the re2c state machine in ext/json's
             * decoder; collapse the maximal-subpart advance to 1 here. */
            if (!encode_advance) {
                adv = 1;
            }
            if (substitute) {
                *w++ = (char)0xEF;
                *w++ = (char)0xBF;
                *w++ = (char)0xBD;
            }
        }
        i += adv;
    }
    *w = '\0';
    return (size_t)(w - out);
}

char *fastjson_sanitize_utf8(const char *s, size_t len, zend_long flags,
                             fj_sanitize_mode mode, size_t *out_len)
{
    /* Precedence asymmetry: encode prefers IGNORE on BOTH-bits, decode
     * prefers SUBSTITUTE. */
    bool has_ignore = (flags & FASTJSON_INVALID_UTF8_IGNORE) != 0;
    bool has_subst  = (flags & FASTJSON_INVALID_UTF8_SUBSTITUTE) != 0;
    bool substitute = (mode == FJ_SAN_DECODE)
        ? has_subst
        : (has_subst && !has_ignore);

    char *out = substitute ? safe_emalloc(len, 3, 1) : safe_emalloc(len, 1, 1);
    *out_len = fastjson_sanitize_utf8_into(s, len, substitute,
                                           mode == FJ_SAN_ENCODE, out);
    return out;
}

zend_string *fastjson_sanitize_utf8_zstr(const char *s, size_t len,
                                         zend_long flags,
                                         fj_sanitize_mode mode)
{
    bool has_ignore = (flags & FASTJSON_INVALID_UTF8_IGNORE) != 0;
    bool has_subst  = (flags & FASTJSON_INVALID_UTF8_SUBSTITUTE) != 0;
    bool substitute = (mode == FJ_SAN_DECODE)
        ? has_subst
        : (has_subst && !has_ignore);

    if (substitute && UNEXPECTED(len > ZSTR_MAX_LEN / 3)) {
        /* Let safe_emalloc raise the standard allocation overflow error. */
        size_t sane_len;
        char *tmp = fastjson_sanitize_utf8(s, len, flags, mode, &sane_len);
        zend_string *zs = zend_string_init(tmp, sane_len, 0);
        efree(tmp);
        return zs;
    }
    size_t cap = substitute ? len * 3 : len;
    zend_string *zs = zend_string_alloc(cap, 0);
    if (UNEXPECTED(fastjson_oom_pending())) {
        zend_string_release_ex(zs, 0);
        return NULL;
    }
    size_t actual = fastjson_sanitize_utf8_into(
        s, len, substitute, mode == FJ_SAN_ENCODE, ZSTR_VAL(zs));
    if (actual == 0) {
        zend_string_release_ex(zs, 0);
        return ZSTR_EMPTY_ALLOC();
    }
    if (actual == 1) {
        unsigned char c = (unsigned char)ZSTR_VAL(zs)[0];
        zend_string_release_ex(zs, 0);
        return ZSTR_CHAR(c);
    }
    ZSTR_LEN(zs) = actual;
    if (cap - actual >= 4096) {
        zs = zend_string_realloc(zs, actual, 0);
    }
    return zs;
}

bool fastjson_utf8_well_formed(const char *s, size_t len)
{
    /* Keep validation rules in sync with fastjson_sanitize_utf8. */
    size_t i = 0;
    /* Bulk-skip ASCII runs: defensive IGNORE/SUBSTITUTE callers usually
     * pass clean UTF-8; this avoids a per-byte loop on long strings. */
    while (i + 8 <= len) {
        uint64_t chunk;
        memcpy(&chunk, s + i, sizeof(chunk));
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        chunk = __builtin_bswap64(chunk);
#endif
        if ((chunk & 0x8080808080808080ULL) != 0) {
            break;
        }
        i += 8;
    }
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        if (c < 0x80) {
            i += 1;
        } else if (c < 0xC2) {
            return false;                                    /* lone trail / overlong lead */
        } else if (c < 0xE0) {
            if (i + 1 >= len) return false;
            if (!FJ_UTF8_TRAIL((unsigned char)s[i + 1])) return false;
            /* C2..DF followed by a trail byte is already shortest-form. */
            i += 2;
        } else if (c < 0xF0) {
            if (i + 3 > len) return false;
            if (!FJ_UTF8_TRAIL((unsigned char)s[i + 1])
                    || !FJ_UTF8_TRAIL((unsigned char)s[i + 2])) {
                return false;
            }
            unsigned int cp = ((c & 0x0f) << 12)
                            | (((unsigned char)s[i + 1] & 0x3f) << 6)
                            |  ((unsigned char)s[i + 2] & 0x3f);
            if (cp < 0x800)                       return false; /* non-shortest */
            if (cp >= 0xD800 && cp <= 0xDFFF)     return false; /* surrogate */
            i += 3;
        } else if (c < 0xF5) {
            if (i + 4 > len) return false;
            if (!FJ_UTF8_TRAIL((unsigned char)s[i + 1])
                    || !FJ_UTF8_TRAIL((unsigned char)s[i + 2])
                    || !FJ_UTF8_TRAIL((unsigned char)s[i + 3])) {
                return false;
            }
            unsigned int cp = ((c & 0x07) << 18)
                            | (((unsigned char)s[i + 1] & 0x3f) << 12)
                            | (((unsigned char)s[i + 2] & 0x3f) << 6)
                            |  ((unsigned char)s[i + 3] & 0x3f);
            if (cp < 0x10000 || cp > 0x10FFFF) return false;
            i += 4;
        } else {
            return false;                                    /* 0xF5..0xFF illegal lead */
        }
    }
    return true;
}

#if !defined(__SSE2__) && !(defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__)))
static zend_always_inline bool fastjson_ascii_word_needs_escape(
    uint64_t word, yyjson_write_flag flags)
{
    const uint64_t ones = UINT64_C(0x0101010101010101);
    const uint64_t highs = UINT64_C(0x8080808080808080);
    uint64_t quote = word ^ (ones * (unsigned char)'"');
    uint64_t slash = word ^ (ones * (unsigned char)'\\');
    uint64_t forward = word ^ (ones * (unsigned char)'/');
    bool has_control = ((word - ones * 0x20) & ~word & highs) != 0;
    bool has_quote = ((quote - ones) & ~quote & highs) != 0;
    bool has_slash = ((slash - ones) & ~slash & highs) != 0;
    bool has_forward = (flags & YYJSON_WRITE_ESCAPE_SLASHES)
        && ((forward - ones) & ~forward & highs) != 0;
    return (word & highs) || has_control || has_quote || has_slash
        || has_forward;
}
#endif

/* Write one JSON string in chunks so a large escaped value does not
 * reserve len*6 up front. The scratch buffer is one chunk's worst case
 * and is freed before return. A chunk never splits a UTF-8 sequence. */
static size_t fastjson_json_chunk_len(const char *s, size_t len,
                                      size_t pos, size_t want)
{
    size_t n = want;
    if (n > len - pos) {
        n = len - pos;
    }
    if (n == 0 || pos + n >= len) {
        return n;
    }
    /* s[i - 1] is the lead of the codepoint that touches the cut.
     * Continuations are 10xxxxxx; measuring one of those as a lead
     * always asks for 2 bytes and drops the real lead from the chunk. */
    size_t i = pos + n;
    while (i > pos && ((unsigned char)s[i - 1] & 0xC0) == 0x80) {
        i--;
    }
    if (i == pos) {
        return n;
    }
    unsigned char lead = (unsigned char)s[i - 1];
    if (lead < 0x80) {
        return n;
    }
    size_t need = lead < 0xE0 ? 2 : (lead < 0xF0 ? 3 : 4);
    size_t have = (pos + n) - (i - 1);
    if (have >= need) {
        return n;
    }
    if (i - 1 > pos) {
        return (i - 1) - pos;
    }
    if (pos + need <= len) {
        return need;
    }
    return n;
}

/* Each chunk is written in place: yyjson's opening quote lands on the
 * byte before the output end and is restored afterwards, so its body
 * starts over the previous chunk's closing quote. The buffer only needs
 * one chunk's 6x worst case of headroom, grown geometrically so heavy
 * escaping does not realloc once per chunk. */
static fj_string_size_status fastjson_write_json_string_chunked(
    smart_str *buf, const char *s, size_t len, yyjson_write_flag flags)
{
    size_t current = buf->s ? ZSTR_LEN(buf->s) : 0;
    enum { CHUNK = 2048 };
    if (UNEXPECTED(current > ZSTR_MAX_LEN - 2
            || len > ZSTR_MAX_LEN - current - 2)) {
        return FJ_STRING_SIZE_TOO_LARGE;
    }
    if (UNEXPECTED(ZSTR_MAX_LEN - current - 2 - len < (size_t)CHUNK * 6)) {
        return FJ_STRING_SIZE_TOO_LARGE;
    }
    smart_str_alloc(buf, len + 2 + (size_t)CHUNK * 6, 0);
    size_t out = current;
    ZSTR_VAL(buf->s)[out++] = '"';
    ZSTR_LEN(buf->s) = out;

    size_t pos = 0;
    while (pos < len) {
        size_t n = fastjson_json_chunk_len(s, len, pos, CHUNK);
        if (UNEXPECTED(n == 0)) {
            ZSTR_LEN(buf->s) = current;
            return FJ_STRING_SIZE_INVALID_UTF8;
        }
        size_t need = n * 6 + 2;
        if (UNEXPECTED(need > ZSTR_MAX_LEN - out)) {
            ZSTR_LEN(buf->s) = current;
            return FJ_STRING_SIZE_TOO_LARGE;
        }
        if (buf->a - out < need) {
            /* Project the rest at the expansion ratio seen so far, plus
             * an eighth, so heavy escaping reallocates about once. The
             * 6x ceiling keeps the total at or under a one-pass reserve. */
            double left = (double)(len - pos - n);
            double ratio = pos ? (double)(out - current) / (double)pos : 1.0;
            double rest = left * ratio * 1.125;
            if (rest > left * 6) {
                rest = left * 6;
            }
            double want = (double)need + rest;
            size_t grow = want >= (double)(ZSTR_MAX_LEN - out)
                ? ZSTR_MAX_LEN - out : (size_t)want;
            smart_str_alloc(buf, grow, 0);
        }
        char *at = ZSTR_VAL(buf->s) + out - 1;
        char saved = *at;
        char *end = yyjson_write_string_to_buf(at, s + pos, n, flags);
        *at = saved;
        if (UNEXPECTED(end == NULL || end - at < 2)) {
            ZSTR_LEN(buf->s) = current;
            return FJ_STRING_SIZE_INVALID_UTF8;
        }
        /* Body without the quotes, now ending on the closing quote. */
        out += (size_t)(end - at) - 2;
        ZSTR_LEN(buf->s) = out;
        pos += n;
    }
    /* The last chunk left its closing quote at `out`. */
    ZSTR_VAL(buf->s)[out++] = '"';
    ZSTR_LEN(buf->s) = out;
    return FJ_STRING_SIZE_OK;
}

static bool fastjson_string_is_copyable_ascii(const char *s, size_t len,
                                              yyjson_write_flag flags,
                                              size_t *prefix_len)
{
    bool escape_slashes = (flags & YYJSON_WRITE_ESCAPE_SLASHES) != 0;
    size_t pos = 0;
#if defined(__SSE2__)
    if (len >= 16) {
        const __m128i v_min = _mm_set1_epi8(0x20);
        const __m128i v_quot = _mm_set1_epi8('"');
        const __m128i v_bsl = _mm_set1_epi8('\\');
        const __m128i v_fsl = _mm_set1_epi8('/');
        for (; pos + 16 <= len; pos += 16) {
            __m128i v = _mm_loadu_si128((const __m128i *)(s + pos));
            __m128i bad = _mm_cmplt_epi8(v, v_min);
            bad = _mm_or_si128(bad, _mm_cmpeq_epi8(v, v_quot));
            bad = _mm_or_si128(bad, _mm_cmpeq_epi8(v, v_bsl));
            if (escape_slashes) {
                bad = _mm_or_si128(bad, _mm_cmpeq_epi8(v, v_fsl));
            }
            int mask = _mm_movemask_epi8(bad);
            if (UNEXPECTED(mask != 0)) {
                *prefix_len = pos + (size_t)zend_ulong_ntz((zend_ulong)(unsigned int)mask);
                return false;
            }
        }
    }
#elif defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
    if (len >= 16) {
        const uint8x16_t v_sub = vdupq_n_u8(0x20);
        const uint8x16_t v_lim = vdupq_n_u8(0x5F);
        const uint8x16_t v_quot = vdupq_n_u8('"');
        const uint8x16_t v_bsl = vdupq_n_u8('\\');
        const uint8x16_t v_fsl = vdupq_n_u8('/');
        for (; pos + 16 <= len; pos += 16) {
            uint8x16_t v = vld1q_u8((const uint8_t *)(s + pos));
            uint8x16_t bad = vcgtq_u8(vsubq_u8(v, v_sub), v_lim);
            bad = vorrq_u8(bad, vceqq_u8(v, v_quot));
            bad = vorrq_u8(bad, vceqq_u8(v, v_bsl));
            if (escape_slashes) {
                bad = vorrq_u8(bad, vceqq_u8(v, v_fsl));
            }
            if (UNEXPECTED(vmaxvq_u8(bad) != 0)) {
                break;
            }
        }
    }
#else
    for (; pos + sizeof(uint64_t) <= len; pos += sizeof(uint64_t)) {
        uint64_t word;
        memcpy(&word, s + pos, sizeof(word));
        if (UNEXPECTED(fastjson_ascii_word_needs_escape(word, flags))) {
            break;
        }
    }
#endif
    uint8_t esc_mask = escape_slashes ? 3u : 1u;
    for (; pos < len; pos++) {
        if ((fastjson_str_esc_tbl[(unsigned char)s[pos]] & esc_mask) != 0) {
            *prefix_len = pos;
            return false;
        }
    }
    *prefix_len = len;
    return true;
}

static fj_string_size_status fastjson_json_string_size_from(
    const char *s, size_t len, size_t prefix_len,
    yyjson_write_flag flags, size_t *out_len)
{
    if (UNEXPECTED(prefix_len > ZSTR_MAX_LEN - 2)) {
        return FJ_STRING_SIZE_TOO_LARGE;
    }
    size_t size = prefix_len + 2;
    size_t i = prefix_len;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        size_t add;
        size_t advance = 1;

        if (c < 0x20) {
            add = (c == '\b' || c == '\t' || c == '\n'
                || c == '\f' || c == '\r') ? 2 : 6;
        } else if (c == '"' || c == '\\') {
            add = 2;
        } else if (c == '/' && (flags & YYJSON_WRITE_ESCAPE_SLASHES)) {
            add = 2;
        } else if (c < 0x80) {
            add = 1;
        } else {
            if (!fastjson_byte_is_valid_utf8_start(s, len, i)) {
                return FJ_STRING_SIZE_INVALID_UTF8;
            }
            advance = c < 0xE0 ? 2 : (c < 0xF0 ? 3 : 4);
            if (flags & YYJSON_WRITE_ESCAPE_UNICODE) {
                add = advance == 4 ? 12 : 6;
            } else {
                add = advance;
            }
        }

        if (UNEXPECTED(add > ZSTR_MAX_LEN - size)) {
            return FJ_STRING_SIZE_TOO_LARGE;
        }
        size += add;
        i += advance;
    }
    *out_len = size;
    return FJ_STRING_SIZE_OK;
}

fj_string_size_status fastjson_json_string_size(const char *s, size_t len,
                                                 yyjson_write_flag flags,
                                                 size_t *out_len,
                                                 bool *copyable_ascii)
{
    size_t prefix_len;
    if (fastjson_string_is_copyable_ascii(s, len, flags, &prefix_len)) {
        if (UNEXPECTED(len > ZSTR_MAX_LEN - 2)) {
            return FJ_STRING_SIZE_TOO_LARGE;
        }
        *out_len = len + 2;
        *copyable_ascii = true;
        return FJ_STRING_SIZE_OK;
    }
    *copyable_ascii = false;
    return fastjson_json_string_size_from(s, len, prefix_len, flags, out_len);
}

fj_string_size_status fastjson_write_large_json_string(
    smart_str *buf, const char *s, size_t len, yyjson_write_flag flags)
{
    size_t current = buf->s ? ZSTR_LEN(buf->s) : 0;
    if (UNEXPECTED(current > ZSTR_MAX_LEN - 2
            || len > ZSTR_MAX_LEN - current - 2)) {
        return FJ_STRING_SIZE_TOO_LARGE;
    }

    bool escape_slashes = (flags & YYJSON_WRITE_ESCAPE_SLASHES) != 0;
    if (len < FASTJSON_EXACT_NONASCII_THRESHOLD) {
        size_t prefix_len;
        if (fastjson_string_is_copyable_ascii(
                s, len, flags, &prefix_len)) {
            smart_str_alloc(buf, len + 2, 0);
            char *cur = ZSTR_VAL(buf->s) + current;
            cur[0] = '"';
            memcpy(cur + 1, s, len);
            cur[len + 1] = '"';
            ZSTR_LEN(buf->s) = current + len + 2;
            return FJ_STRING_SIZE_OK;
        }
        if (prefix_len >= len - len / 8) {
            size_t tail_len = len - prefix_len;
            if (UNEXPECTED(tail_len >
                    (ZSTR_MAX_LEN - current - prefix_len - 2) / 6)) {
                return FJ_STRING_SIZE_TOO_LARGE;
            }
            smart_str_alloc(buf, prefix_len + tail_len * 6 + 2, 0);
            char *cur = ZSTR_VAL(buf->s) + current;
            cur[0] = '"';
            memcpy(cur + 1, s, prefix_len);
            /* Write yyjson's opening quote over the last copied prefix byte
             * (or opening quote if prefix_len == 0) and restore it, avoiding memmove. */
            char *at = cur + prefix_len;
            char saved = *at;
            char *end = yyjson_write_string_to_buf(
                at, s + prefix_len, tail_len, flags);
            *at = saved;
            if (UNEXPECTED(end == NULL)) {
                return FJ_STRING_SIZE_INVALID_UTF8;
            }
            ZSTR_LEN(buf->s) = (size_t)(end - ZSTR_VAL(buf->s));
            return FJ_STRING_SIZE_OK;
        }
        return fastjson_write_json_string_chunked(buf, s, len, flags);
    }

    /* Fuse SIMD validation and copying for >= 8 MiB strings. On escape/non-ASCII
     * at pos, measure and emit only the remaining tail [pos..len) without
     * re-scanning or re-copying the verified prefix [0..pos). */
    smart_str_alloc(buf, len + 2, 0);
    char *cur = ZSTR_VAL(buf->s) + current;
    cur[0] = '"';
    size_t pos = fastjson_copy_clean_ascii(cur + 1, s, len, escape_slashes);
    if (pos == len) {
        cur[len + 1] = '"';
        ZSTR_LEN(buf->s) = current + len + 2;
        return FJ_STRING_SIZE_OK;
    }

    size_t exact_len;
    fj_string_size_status status = fastjson_json_string_size_from(
        s, len, pos, flags, &exact_len);
    if (status != FJ_STRING_SIZE_OK) {
        return status;
    }
    if (UNEXPECTED(exact_len > ZSTR_MAX_LEN - current)) {
        return FJ_STRING_SIZE_TOO_LARGE;
    }
    /* Advance ZSTR_LEN over opening quote + verified prefix so reallocation preserves them. */
    ZSTR_LEN(buf->s) = current + 1 + pos;
    smart_str_alloc(buf, exact_len - (1 + pos), 0);
    cur = ZSTR_VAL(buf->s) + current;
    char *at = cur + pos;
    char saved = *at;
    char *end = yyjson_write_string_to_buf(at, s + pos, len - pos, flags);
    *at = saved;
    if (UNEXPECTED(end == NULL)) {
        ZSTR_LEN(buf->s) = current;
        return FJ_STRING_SIZE_INVALID_UTF8;
    }
    ZSTR_LEN(buf->s) = (size_t)(end - ZSTR_VAL(buf->s));
    return FJ_STRING_SIZE_OK;
}

bool fastjson_apply_hex_escapes(smart_str *buf, zend_long flags,
                                size_t start_pos)
{
    bool tag = flags & FASTJSON_ENCODE_HEX_TAG;
    bool amp = flags & FASTJSON_ENCODE_HEX_AMP;
    bool apos = flags & FASTJSON_ENCODE_HEX_APOS;
    bool quot = flags & FASTJSON_ENCODE_HEX_QUOT;
    size_t orig_len = ZSTR_LEN(buf->s) - start_pos;

    if (orig_len == 0) {
        return true;
    }

    const char *src = ZSTR_VAL(buf->s) + start_pos;
    size_t growth = 0;
    size_t i = 0;
#if defined(__SSE2__)
    const __m128i v_bsl = _mm_set1_epi8('\\');
    const __m128i v_lt = _mm_set1_epi8('<');
    const __m128i v_gt = _mm_set1_epi8('>');
    const __m128i v_amp = _mm_set1_epi8('&');
    const __m128i v_apos = _mm_set1_epi8('\'');
    size_t scalar_stride = 16;
#elif defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
    const uint8x16_t v_bsl = vdupq_n_u8('\\');
    const uint8x16_t v_lt = vdupq_n_u8('<');
    const uint8x16_t v_gt = vdupq_n_u8('>');
    const uint8x16_t v_amp = vdupq_n_u8('&');
    const uint8x16_t v_apos = vdupq_n_u8('\'');
    size_t scalar_stride = 16;
#endif
    while (i < orig_len) {
#if defined(__SSE2__)
        size_t simd_start = i;
        while (i + 16 <= orig_len) {
            __m128i v = _mm_loadu_si128((const __m128i *)(src + i));
            __m128i hit = _mm_cmpeq_epi8(v, v_bsl);
            hit = _mm_or_si128(hit, _mm_cmpeq_epi8(v, v_lt));
            hit = _mm_or_si128(hit, _mm_cmpeq_epi8(v, v_gt));
            hit = _mm_or_si128(hit, _mm_cmpeq_epi8(v, v_amp));
            hit = _mm_or_si128(hit, _mm_cmpeq_epi8(v, v_apos));
            if (_mm_movemask_epi8(hit) != 0) {
                break;
            }
            i += 16;
        }
        if (i >= orig_len) break;
        if (i > simd_start) {
            scalar_stride = 16;
        }
        size_t rem = orig_len - i;
        size_t block_end = i + (rem < scalar_stride ? rem : scalar_stride);
        if (scalar_stride < 256) {
            scalar_stride <<= 1;
        }
#elif defined(__aarch64__) && (defined(__ARM_NEON) || defined(__ARM_NEON__))
        size_t simd_start = i;
        while (i + 16 <= orig_len) {
            uint8x16_t v = vld1q_u8((const uint8_t *)(src + i));
            uint8x16_t hit = vceqq_u8(v, v_bsl);
            hit = vorrq_u8(hit, vceqq_u8(v, v_lt));
            hit = vorrq_u8(hit, vceqq_u8(v, v_gt));
            hit = vorrq_u8(hit, vceqq_u8(v, v_amp));
            hit = vorrq_u8(hit, vceqq_u8(v, v_apos));
            if (vmaxvq_u8(hit) != 0) {
                break;
            }
            i += 16;
        }
        if (i >= orig_len) break;
        if (i > simd_start) {
            scalar_stride = 16;
        }
        size_t rem = orig_len - i;
        size_t block_end = i + (rem < scalar_stride ? rem : scalar_stride);
        if (scalar_stride < 256) {
            scalar_stride <<= 1;
        }
#else
        size_t block_end = orig_len;
#endif
        for (; i < block_end; i++) {
            unsigned char c = (unsigned char)src[i];
            if (c == '\\' && i + 1 < orig_len) {
                unsigned char next = (unsigned char)src[i + 1];
                if (quot && next == '"') {
                    if (UNEXPECTED(growth > ZSTR_MAX_LEN - 4)) {
                        return false;
                    }
                    growth += 4;
                    i++;
                    continue;
                }
                i += next == 'u' && i + 5 < orig_len ? 5 : 1;
                continue;
            }
            if ((tag && (c == '<' || c == '>'))
                    || (amp && c == '&') || (apos && c == '\'')) {
                if (UNEXPECTED(growth > ZSTR_MAX_LEN - 5)) {
                    return false;
                }
                growth += 5;
            }
        }
    }
    if (growth == 0) {
        return true;
    }
    if (UNEXPECTED(growth > ZSTR_MAX_LEN - start_pos - orig_len)) {
        return false;
    }

    smart_str_alloc(buf, growth, 0);
    char *bytes = ZSTR_VAL(buf->s);
    size_t read = start_pos + orig_len;
    size_t write = read + growth;

    /* Rewrite backwards so expanding a byte cannot overwrite unread input. */
    while (read > start_pos) {
        size_t pos = --read;
        unsigned char c = (unsigned char)bytes[pos];
        const char *replacement = NULL;

        if (quot && c == '"' && pos > start_pos
                && pos + 1 < start_pos + orig_len
                && bytes[pos - 1] == '\\') {
            replacement = "\\u0022";
            read--;
        } else if (tag && c == '<') {
            replacement = "\\u003C";
        } else if (tag && c == '>') {
            replacement = "\\u003E";
        } else if (amp && c == '&') {
            replacement = "\\u0026";
        } else if (apos && c == '\'') {
            replacement = "\\u0027";
        }

        if (replacement != NULL) {
            write -= 6;
            memcpy(bytes + write, replacement, 6);
        } else {
            bytes[--write] = (char)c;
        }
    }
    ZEND_ASSERT(write == start_pos);
    ZSTR_LEN(buf->s) = start_pos + orig_len + growth;
    return true;
}

bool fastjson_input_has_inf_nan_literal(const char *s, size_t len,
                                        bool allow_comments)
{
    /* Skip strings and JSONC comments: "info" inside a comment must not
     * block overflow retry, nor may a comment's quote hide a bare Inf token. */
    bool in_str = false;
    bool escape = false;
    for (size_t i = 0; i < len; i++) {
        unsigned char c = (unsigned char)s[i];
        if (in_str) {
            if (escape) { escape = false; continue; }
            if (c == '\\') { escape = true; continue; }
            if (c == '"') { in_str = false; }
            continue;
        }
        if (allow_comments && c == '/' && i + 1 < len) {
            if (s[i + 1] == '/') {
                i += 2;
                /* yyjson terminates a line comment on CR or LF
                 * (char_is_eol). Stop on either so a CR-terminated
                 * comment doesn't swallow the rest of the input. */
                while (i < len && s[i] != '\n' && s[i] != '\r') i++;
                continue;
            }
            if (s[i + 1] == '*') {
                i += 2;
                while (i + 1 < len && !(s[i] == '*' && s[i + 1] == '/')) i++;
                i++; /* land on '/'; loop's i++ steps past it */
                continue;
            }
        }
        if (c == '"') { in_str = true; continue; }
        unsigned char lc = c | 0x20;
        if (lc == 'i' && i + 2 < len
                && ((unsigned char)s[i + 1] | 0x20) == 'n'
                && ((unsigned char)s[i + 2] | 0x20) == 'f') {
            return true;
        }
        if (lc == 'n' && i + 2 < len
                && ((unsigned char)s[i + 1] | 0x20) == 'a'
                && ((unsigned char)s[i + 2] | 0x20) == 'n') {
            return true;
        }
    }
    return false;
}

PHP_FUNCTION(fastjson_version)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_STRING(PHP_FASTJSON_VERSION);
}

#if PHP_VERSION_ID >= 80400
ZEND_FRAMELESS_FUNCTION(fastjson_version, 0)
{
    RETURN_STRING(PHP_FASTJSON_VERSION);
}
#endif

static void fastjson_validate_impl(const char *json, size_t json_len,
                                   zend_long depth, zend_long flags,
                                   zval *return_value)
{
    /* Match ext/json's validation order: flags, empty input, then depth.
     * Empty input returns false even when depth would raise ValueError. */
    if (UNEXPECTED(flags & ~(zend_long)FASTJSON_DECODE_INVALID_UTF8_IGNORE)) {
        zend_argument_value_error(3,
            "must be a valid flag (allowed flags: JSON_INVALID_UTF8_IGNORE)");
        RETURN_THROWS();
    }

    if (UNEXPECTED(json_len == 0)) {
        /* Same message yyjson produces, so the fast path is
         * indistinguishable from parsing the empty input. */
        yyjson_read_err err = {
            YYJSON_READ_ERROR_INVALID_PARAMETER,
            "input length is 0",
            0
        };
        fastjson_set_read_error(json, json_len, &err);
        RETURN_FALSE;
    }

    /* Depth ValueErrors must leave last_error as NONE, matching ext/json. */
    fastjson_clear_error();

    if (UNEXPECTED(depth <= 0)) {
        zend_argument_value_error(2, "must be greater than 0");
        RETURN_THROWS();
    }
    if (UNEXPECTED(depth > INT_MAX)) {
        zend_argument_value_error(2, "must be less than %d", INT_MAX);
        RETURN_THROWS();
    }

    /* P-002/P-008 validate-only allocates only a 64-byte stub doc (plus a
     * depth stack when nesting > 32), independent of input length; serve it
     * from the stack bump allocator for zero Zend-heap traffic at any size. */
    yyjson_read_err err;
    fastjson_small_alc small_alc;
    fastjson_small_alc_init(&small_alc);
    yyjson_doc *doc = fastjson_read_doc_alc(json, json_len, flags,
                                            YYJSON_READ_VALIDATE_ONLY,
                                            &small_alc.alc, &err);
    if (UNEXPECTED(doc == NULL)) {
        fastjson_set_read_error(json, json_len, &err);
        RETURN_FALSE;
    }

    /* The validate stub's val_read is the container nesting counted
     * during the parse (root counts as 1, a top-level scalar leaves
     * it 0). The stub has no root and must not be walked. */
    size_t nesting = doc->val_read;
    if (UNEXPECTED(nesting >= (size_t)depth)) {
        yyjson_doc_free(doc);
        fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                "Maximum stack depth exceeded");
        RETURN_FALSE;
    }

    yyjson_doc_free(doc);
    fastjson_clear_error();
    RETURN_TRUE;
}

PHP_FUNCTION(fastjson_validate)
{
    char *json;
    size_t json_len;
    zend_long depth = 512;
    zend_long flags = 0;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_STRING(json, json_len)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(depth)
        Z_PARAM_LONG(flags)
    ZEND_PARSE_PARAMETERS_END();

    fastjson_validate_impl(json, json_len, depth, flags, return_value);
}

#if PHP_VERSION_ID >= 80400
ZEND_FRAMELESS_FUNCTION(fastjson_validate, 1)
{
    zval json_tmp;
    zend_string *json;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    fastjson_validate_impl(ZSTR_VAL(json), ZSTR_LEN(json), 512, 0, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
}

ZEND_FRAMELESS_FUNCTION(fastjson_validate, 2)
{
    zval json_tmp;
    zend_string *json;
    zend_long depth;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    Z_FLF_PARAM_LONG(2, depth);
    fastjson_validate_impl(ZSTR_VAL(json), ZSTR_LEN(json), depth, 0, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
}

ZEND_FRAMELESS_FUNCTION(fastjson_validate, 3)
{
    zval json_tmp;
    zend_string *json;
    zend_long depth;
    zend_long flags;

    Z_FLF_PARAM_STR(1, json, json_tmp);
    Z_FLF_PARAM_LONG(2, depth);
    Z_FLF_PARAM_LONG(3, flags);
    fastjson_validate_impl(ZSTR_VAL(json), ZSTR_LEN(json), depth, flags, return_value);

flf_clean:
    Z_FLF_PARAM_FREE_STR(1, json_tmp);
}
#endif

PHP_FUNCTION(fastjson_last_error)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(FASTJSON_G(last_err_code));
}

PHP_FUNCTION(fastjson_last_error_msg)
{
    ZEND_PARSE_PARAMETERS_NONE();

    if (FASTJSON_G(last_err_code) == FASTJSON_ERROR_NONE
            || FASTJSON_G(last_err_msg) == NULL) {
        RETURN_STRING("No error");
    }

    RETURN_STRING(FASTJSON_G(last_err_msg));
}

PHP_FUNCTION(fastjson_last_error_pos)
{
    ZEND_PARSE_PARAMETERS_NONE();
    RETURN_LONG(FASTJSON_G(last_err_pos));
}

static void fastjson_last_error_info_impl(zval *return_value)
{
    array_init_size(return_value, 5);
    add_assoc_long(return_value, "code", FASTJSON_G(last_err_code));
    if (FASTJSON_G(last_err_code) == FASTJSON_ERROR_NONE
            || FASTJSON_G(last_err_msg) == NULL) {
        add_assoc_string(return_value, "msg", "No error");
    } else {
        add_assoc_string(return_value, "msg", FASTJSON_G(last_err_msg));
    }
    add_assoc_long(return_value, "pos", FASTJSON_G(last_err_pos));
    add_assoc_long(return_value, "line", FASTJSON_G(last_err_line));
    add_assoc_long(return_value, "col", FASTJSON_G(last_err_col));
}

PHP_FUNCTION(fastjson_last_error_info)
{
    ZEND_PARSE_PARAMETERS_NONE();
    fastjson_last_error_info_impl(return_value);
}

#if PHP_VERSION_ID >= 80400
ZEND_FRAMELESS_FUNCTION(fastjson_last_error, 0)
{
    RETURN_LONG(FASTJSON_G(last_err_code));
}

ZEND_FRAMELESS_FUNCTION(fastjson_last_error_msg, 0)
{
    if (FASTJSON_G(last_err_code) == FASTJSON_ERROR_NONE
            || FASTJSON_G(last_err_msg) == NULL) {
        RETURN_STRING("No error");
    }
    RETURN_STRING(FASTJSON_G(last_err_msg));
}

ZEND_FRAMELESS_FUNCTION(fastjson_last_error_pos, 0)
{
    RETURN_LONG(FASTJSON_G(last_err_pos));
}

ZEND_FRAMELESS_FUNCTION(fastjson_last_error_info, 0)
{
    fastjson_last_error_info_impl(return_value);
}
#endif

PHP_GINIT_FUNCTION(fastjson)
{
#if defined(COMPILE_DL_FASTJSON) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    fastjson_globals->last_err_code = FASTJSON_ERROR_NONE;
    fastjson_globals->last_err_msg = NULL;
    fastjson_globals->last_err_pos = -1;
    fastjson_globals->last_err_line = 0;
    fastjson_globals->last_err_col = 0;
    fastjson_globals->key_cache = NULL;
    fastjson_globals->key_cache_in_use = false;
}

PHP_GSHUTDOWN_FUNCTION(fastjson)
{
    if (fastjson_globals->key_cache != NULL) {
        fastjson_key_cache_destroy(fastjson_globals->key_cache);
        fastjson_globals->key_cache = NULL;
    }
    fastjson_globals->key_cache_in_use = false;
}

PHP_RINIT_FUNCTION(fastjson)
{
#if defined(COMPILE_DL_FASTJSON) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    /* Module globals persist across requests in non-ZTS builds; without a
     * reset, one request's error state would leak into the next. */
    fastjson_clear_error();
    fastjson_key_cache_rinit();
    return SUCCESS;
}

PHP_RSHUTDOWN_FUNCTION(fastjson)
{
    fastjson_key_cache_rshutdown();
    return SUCCESS;
}

PHP_MINIT_FUNCTION(fastjson)
{
#if PHP_VERSION_ID >= 80300 && !defined(PHP_WIN32)
    /* Headers from a newer patch can make inline helpers call symbols an older
     * patch does not export; ELF and Mach-O bind them lazily, so without this
     * check the process dies on first use instead of at startup. Windows
     * resolves imports at load time and fails there already. */
    if (php_version_id() < PHP_VERSION_ID) {
        php_error_docref(NULL, E_CORE_WARNING,
            "fastjson was built for PHP " PHP_VERSION " and cannot run on PHP %s; "
            "install a build for this PHP version or build from source",
            php_version());
        return FAILURE;
    }
#endif
    register_fastjson_symbols(module_number);

    /* The optional dependency orders ext/json's MINIT before these lookups. */
    fastjson_json_exception_ce = zend_hash_str_find_ptr(CG(class_table),
        "jsonexception", sizeof("jsonexception") - 1);
    if (fastjson_json_exception_ce == NULL) {
        /* Without ext/json, throw a fastjson-owned \Exception subclass so
         * callers can catch it specifically. */
        zend_class_entry ce;
        INIT_CLASS_ENTRY(ce, "Fastjson\\JsonException", NULL);
        fastjson_json_exception_ce =
            zend_register_internal_class_ex(&ce, zend_ce_exception);
    }
    fastjson_json_serializable_ce = zend_hash_str_find_ptr(CG(class_table),
        "jsonserializable", sizeof("jsonserializable") - 1);
    fastjson_jsonserialize_str = zend_string_init_interned(
        "jsonserialize", sizeof("jsonserialize") - 1, 1);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(fastjson)
{
    fastjson_jsonserialize_str = NULL;
    return SUCCESS;
}

PHP_MINFO_FUNCTION(fastjson)
{
    php_info_print_table_start();
    php_info_print_table_header(2, "fastjson support", "enabled");
    php_info_print_table_row(2, "Version", PHP_FASTJSON_VERSION);
    php_info_print_table_row(2, "Backend", "yyjson " YYJSON_VERSION_STRING);
    php_info_print_table_end();
}

/* Permit loading without ext/json, using the MINIT fallback class. */
static const zend_module_dep fastjson_deps[] = {
    ZEND_MOD_OPTIONAL("json")
    ZEND_MOD_END
};

zend_module_entry fastjson_module_entry = {
    STANDARD_MODULE_HEADER_EX, NULL, fastjson_deps,
    "fastjson",
    ext_functions,
    PHP_MINIT(fastjson),
    PHP_MSHUTDOWN(fastjson),
    PHP_RINIT(fastjson),
    PHP_RSHUTDOWN(fastjson),
    PHP_MINFO(fastjson),
    PHP_FASTJSON_VERSION,
    PHP_MODULE_GLOBALS(fastjson),
    PHP_GINIT(fastjson),
    PHP_GSHUTDOWN(fastjson),
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_FASTJSON
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(fastjson)
#endif
