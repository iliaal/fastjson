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
#include "php_fastjson.h"
#include "fastjson_arginfo.h"
#include "yyjson.h"
#include "fastjson_alloc.h"

ZEND_DECLARE_MODULE_GLOBALS(fastjson)

zend_class_entry *fastjson_json_exception_ce = NULL;
zend_class_entry *fastjson_json_serializable_ce = NULL;

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

void fastjson_set_error(yyjson_read_code code, const char *msg)
{
    FASTJSON_G(last_err_code) = fastjson_translate_read_code(code);
    FASTJSON_G(last_err_msg) = msg;
    /* No source buffer at this call site; report "location unknown". */
    FASTJSON_G(last_err_pos) = -1;
    FASTJSON_G(last_err_line) = 0;
    FASTJSON_G(last_err_col) = 0;
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

void fastjson_clear_error(void)
{
    FASTJSON_G(last_err_code) = FASTJSON_ERROR_NONE;
    FASTJSON_G(last_err_msg) = NULL;
    FASTJSON_G(last_err_pos) = -1;
    FASTJSON_G(last_err_line) = 0;
    FASTJSON_G(last_err_col) = 0;
}

void fastjson_set_error_code(zend_long code, const char *msg)
{
    FASTJSON_G(last_err_code) = code;
    FASTJSON_G(last_err_msg) = msg;
    /* Encode/IO/depth errors carry no source-byte offset. */
    FASTJSON_G(last_err_pos) = -1;
    FASTJSON_G(last_err_line) = 0;
    FASTJSON_G(last_err_col) = 0;
}

void fastjson_save_error_state(fastjson_error_state *state)
{
    state->code = FASTJSON_G(last_err_code);
    state->msg = FASTJSON_G(last_err_msg);
    state->pos = FASTJSON_G(last_err_pos);
    state->line = FASTJSON_G(last_err_line);
    state->col = FASTJSON_G(last_err_col);
}

void fastjson_restore_error_state(const fastjson_error_state *state)
{
    FASTJSON_G(last_err_code) = state->code;
    FASTJSON_G(last_err_msg) = state->msg;
    FASTJSON_G(last_err_pos) = state->pos;
    FASTJSON_G(last_err_line) = state->line;
    FASTJSON_G(last_err_col) = state->col;
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

/* Inline helpers mirroring ext/standard/html.c's UTF-8 advance rules.
 * Keeping the helpers static avoids cross-TU calls in the inner loop. */
#define FJ_UTF8_TRAIL(c) ((c) >= 0x80 && (c) <= 0xBF)
#define FJ_UTF8_LEAD(c)  ((c) < 0x80 || ((c) >= 0xC2 && (c) <= 0xF4))

char *fastjson_sanitize_utf8(const char *s, size_t len, zend_long flags,
                             fj_sanitize_mode mode, size_t *out_len)
{
    /* Precedence asymmetry: encode prefers IGNORE on BOTH-bits, decode
     * prefers SUBSTITUTE. */
    bool has_ignore = (flags & FASTJSON_INVALID_UTF8_IGNORE) != 0;
    bool has_subst  = (flags & FASTJSON_INVALID_UTF8_SUBSTITUTE) != 0;
    bool substitute = (mode == FJ_SAN_DECODE)
        ? has_subst                   /* decode: any SUBSTITUTE wins */
        : (has_subst && !has_ignore); /* encode: BOTH -> IGNORE strips */

    /* Worst-case output: 3 bytes per input byte (each invalid byte
     * substituted by the 3-byte U+FFFD). IGNORE-only is <= input-sized.
     * safe_emalloc(nmemb, size, offset) bails the request via PHP's
     * out-of-memory handler if nmemb * size + offset overflows
     * size_t -- which lets us cap any caller-controlled `len` without
     * an explicit (SIZE_MAX-1)/3 check here. */
    char *out;
    if (substitute) {
        out = safe_emalloc(len, 3, 1);
    } else {
        out = emalloc(len > 0 ? len : 1);
    }
    char *w = out;

    /* Encode side: mirror ext/standard/html.c::get_next_char (UTR-36
     * strategy 2 maximal-subpart). Decode side: per-byte advance on
     * invalid (matches the re2c state machine in ext/json's parser).
     * Both paths share the same per-byte SUBSTITUTE / IGNORE emit
     * step, only differing in how far they advance after a bad byte. */
    bool encode_advance = (mode == FJ_SAN_ENCODE);
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        size_t adv;
        bool good;
        if (c < 0x80) {
            adv = 1; good = true;
        } else if (c < 0xC2) {
            /* Lone continuation or 0xC0/0xC1 overlong lead. */
            adv = 1; good = false;
        } else if (c < 0xE0) {
            /* 2-byte sequence. */
            if (i + 1 >= len || !FJ_UTF8_TRAIL((unsigned char)s[i + 1])) {
                adv = (i + 1 >= len || FJ_UTF8_LEAD((unsigned char)s[i + 1])) ? 1 : 2;
                good = false;
            } else {
                unsigned int cp = ((c & 0x1f) << 6) | ((unsigned char)s[i + 1] & 0x3f);
                if (cp < 0x80) { adv = 2; good = false; } /* non-shortest */
                else           { adv = 2; good = true; }
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
    *out_len = (size_t)(w - out);
    return out;
}

bool fastjson_utf8_well_formed(const char *s, size_t len)
{
    /* Mirror fastjson_sanitize_utf8's per-codepoint rules without writing
     * or allocating. Any byte sequence that the sanitizer would have
     * dropped or substituted returns false here; valid sequences return
     * true. Two functions, one ruleset -- keep them in lockstep. */
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
            unsigned int cp = ((c & 0x1f) << 6) | ((unsigned char)s[i + 1] & 0x3f);
            if (cp < 0x80) return false;                     /* non-shortest */
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

static zend_always_inline bool fastjson_ascii_byte_needs_escape(
    unsigned char c, yyjson_write_flag flags)
{
    return c < 0x20 || c >= 0x80 || c == '"' || c == '\\'
        || (c == '/' && (flags & YYJSON_WRITE_ESCAPE_SLASHES));
}

static bool fastjson_string_is_copyable_ascii(const char *s, size_t len,
                                              yyjson_write_flag flags,
                                              size_t *prefix_len)
{
    size_t pos = 0;
    for (; pos + sizeof(uint64_t) <= len; pos += sizeof(uint64_t)) {
        uint64_t word;
        memcpy(&word, s + pos, sizeof(word));
        if (UNEXPECTED(fastjson_ascii_word_needs_escape(word, flags))) {
            if (pos >= len - len / 8) {
                for (; pos < len; pos++) {
                    if (fastjson_ascii_byte_needs_escape(
                            (unsigned char)s[pos], flags)) {
                        break;
                    }
                }
            }
            *prefix_len = pos;
            return false;
        }
    }
    for (; pos < len; pos++) {
        if (fastjson_ascii_byte_needs_escape((unsigned char)s[pos], flags)) {
            *prefix_len = pos;
            return false;
        }
    }
    *prefix_len = len;
    return true;
}

fj_string_size_status fastjson_json_string_size(const char *s, size_t len,
                                                 yyjson_write_flag flags,
                                                 size_t *out_len,
                                                 bool *copyable_ascii)
{
    /* yyjson's common large-string case is printable ASCII without escape
     * candidates. Detect it eight bytes at a time so the exact-size path
     * remains memory-bandwidth-bound instead of adding a branch per byte. */
    size_t prefix_len;
    if (fastjson_string_is_copyable_ascii(s, len, flags, &prefix_len)) {
        if (UNEXPECTED(len > ZSTR_MAX_LEN - 2)) {
            return FJ_STRING_SIZE_TOO_LARGE;
        }
        *out_len = len + 2;
        *copyable_ascii = true;
        return FJ_STRING_SIZE_OK;
    }

    size_t size = 2; /* surrounding quotes */
    bool copyable = true;
    size_t i = 0;
    while (i < len) {
        unsigned char c = (unsigned char)s[i];
        size_t add;
        size_t advance = 1;

        if (c < 0x20) {
            copyable = false;
            add = (c == '\b' || c == '\t' || c == '\n'
                || c == '\f' || c == '\r') ? 2 : 6;
        } else if (c == '"' || c == '\\') {
            copyable = false;
            add = 2;
        } else if (c == '/' && (flags & YYJSON_WRITE_ESCAPE_SLASHES)) {
            copyable = false;
            add = 2;
        } else if (c < 0x80) {
            add = 1;
        } else {
            copyable = false;
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
    *copyable_ascii = copyable;
    return FJ_STRING_SIZE_OK;
}

fj_string_size_status fastjson_write_large_json_string(
    smart_str *buf, const char *s, size_t len, yyjson_write_flag flags)
{
    size_t current = buf->s ? ZSTR_LEN(buf->s) : 0;
    if (UNEXPECTED(current > ZSTR_MAX_LEN - 2
            || len > ZSTR_MAX_LEN - current - 2)) {
        return FJ_STRING_SIZE_TOO_LARGE;
    }

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
                    (ZSTR_MAX_LEN - current - prefix_len - 3) / 6)) {
                return FJ_STRING_SIZE_TOO_LARGE;
            }
            smart_str_alloc(buf, prefix_len + tail_len * 6 + 3, 0);
            char *cur = ZSTR_VAL(buf->s) + current;
            cur[0] = '"';
            memcpy(cur + 1, s, prefix_len);
            char *tail = cur + prefix_len + 1;
            char *end = yyjson_write_string_to_buf(
                tail, s + prefix_len, tail_len, flags);
            if (UNEXPECTED(end == NULL)) {
                return FJ_STRING_SIZE_INVALID_UTF8;
            }
            size_t tail_output_len = (size_t)(end - tail);
            memmove(tail, tail + 1, tail_output_len - 1);
            end--;
            ZSTR_LEN(buf->s) = (size_t)(end - ZSTR_VAL(buf->s));
            return FJ_STRING_SIZE_OK;
        }
        if (UNEXPECTED(len > (ZSTR_MAX_LEN - current - 2) / 6)) {
            return FJ_STRING_SIZE_TOO_LARGE;
        }
        smart_str_alloc(buf, len * 6 + 2, 0);
        char *cur = ZSTR_VAL(buf->s) + current;
        char *end = yyjson_write_string_to_buf(cur, s, len, flags);
        if (UNEXPECTED(end == NULL)) {
            return FJ_STRING_SIZE_INVALID_UTF8;
        }
        ZSTR_LEN(buf->s) = (size_t)(end - ZSTR_VAL(buf->s));
        return FJ_STRING_SIZE_OK;
    }

    /* Fuse validation and copying for the dominant printable-ASCII case.
     * The buffer length remains unchanged until the whole value is known to
     * be copyable, so the slow path can safely overwrite this scratch data. */
    smart_str_alloc(buf, len + 2, 0);
    char *cur = ZSTR_VAL(buf->s) + current;
    cur[0] = '"';
    size_t pos = 0;
    for (; pos + sizeof(uint64_t) <= len; pos += sizeof(uint64_t)) {
        uint64_t word;
        memcpy(&word, s + pos, sizeof(word));
        if (fastjson_ascii_word_needs_escape(word, flags)) {
            break;
        }
        memcpy(cur + pos + 1, &word, sizeof(word));
    }
    for (; pos < len; pos++) {
        unsigned char c = (unsigned char)s[pos];
        if (fastjson_ascii_byte_needs_escape(c, flags)) {
            break;
        }
        cur[pos + 1] = (char)c;
    }
    if (pos == len) {
        cur[len + 1] = '"';
        ZSTR_LEN(buf->s) = current + len + 2;
        return FJ_STRING_SIZE_OK;
    }

    size_t exact_len;
    bool copyable_ascii;
    fj_string_size_status status = fastjson_json_string_size(
        s, len, flags, &exact_len, &copyable_ascii);
    if (status != FJ_STRING_SIZE_OK) {
        return status;
    }
    if (UNEXPECTED(exact_len > ZSTR_MAX_LEN - current)) {
        return FJ_STRING_SIZE_TOO_LARGE;
    }
    smart_str_alloc(buf, exact_len, 0);
    cur = ZSTR_VAL(buf->s) + current;
    char *end;
    if (copyable_ascii) {
        cur[0] = '"';
        memcpy(cur + 1, s, len);
        cur[len + 1] = '"';
        end = cur + len + 2;
    } else {
        end = yyjson_write_string_to_buf(cur, s, len, flags);
        if (UNEXPECTED(end == NULL)) {
            return FJ_STRING_SIZE_INVALID_UTF8;
        }
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
    for (size_t i = 0; i < orig_len; i++) {
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
    /* Walk outside string literals. Inside "..." backslash escapes
     * the next char. Anywhere unquoted, an ASCII run starting with
     * I/i + N/n + F/f (Inf or Infinity) or N/n + A/a + N/n (NaN) is
     * the literal yyjson's ALLOW_INF_AND_NAN accepts and ext/json
     * doesn't. yyjson also accepts a leading '-'; we still match
     * either form via the I/N start.
     *
     * When the caller decodes with FASTJSON_DECODE_RELAXED, yyjson also
     * accepts JSONC line/block comments. Their bytes are grammar, not
     * tokens, so skip them: a comment containing "info" must not read
     * as an Inf literal (false positive that blocks the legitimate
     * exponent-overflow retry), and an unbalanced quote inside a comment
     * must not flip the string state and hide a real bare Inf token
     * outside it (false negative). */
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

    /* Argument validation order mirrors ext/json's json_validate
     * verbatim so behavior matches when multiple inputs are out of
     * spec at once:
     *   1. $flags check     -> ValueError (allowed flags message)
     *   2. empty input      -> RETURN_FALSE with SYNTAX error
     *   3. $depth <= 0      -> ValueError "must be greater than 0"
     *   4. $depth > INT_MAX -> ValueError "must be less than %d"
     * In particular: empty input with bad $depth short-circuits to
     * false before the depth check raises. */
    if (flags & ~(zend_long)FASTJSON_DECODE_INVALID_UTF8_IGNORE) {
        zend_argument_value_error(3,
            "must be a valid flag (allowed flags: JSON_INVALID_UTF8_IGNORE)");
        RETURN_THROWS();
    }

    if (json_len == 0) {
        /* Use the same message yyjson would have produced so the
         * fast-path return is observationally identical to the parse-
         * path return for empty input. */
        yyjson_read_err err = {
            YYJSON_READ_ERROR_INVALID_PARAMETER,
            "input length is 0",
            0
        };
        fastjson_set_read_error(json, json_len, &err);
        RETURN_FALSE;
    }

    /* Clear residual last_error state from a prior call before any
     * argument-validation ValueError can throw. Mirrors ext/json's
     * json_validate, which resets error state immediately after the
     * empty-input short-circuit so the post-throw global state is
     * NONE, not whatever the previous call left behind. */
    fastjson_clear_error();

    if (depth <= 0) {
        zend_argument_value_error(2, "must be greater than 0");
        RETURN_THROWS();
    }
    if (depth > INT_MAX) {
        zend_argument_value_error(2, "must be less than %d", INT_MAX);
        RETURN_THROWS();
    }

    /* Depth enforcement on the success path is intentionally NOT done
     * here -- it would require walking the parsed yyjson_doc, which
     * doubles the success-path cost. yyjson's internal recursion guard
     * still rejects pathological nesting before stack exhaustion. */
    yyjson_read_flag yflags = YYJSON_READ_VALIDATE_ONLY;
    if (flags & FASTJSON_DECODE_INVALID_UTF8_IGNORE) {
        yflags |= YYJSON_READ_ALLOW_INVALID_UNICODE;
    }

    yyjson_read_err err;
    /* yyjson patch P-002: validate-only mode skips val_hdr allocation
     * (~2/3 of peak memory). The returned doc is a stub sentinel; we
     * just check non-NULL and free it. See vendor/yyjson/PATCHES.md. */
    yyjson_doc *doc = yyjson_read_opts(json, json_len, yflags,
                                       &fastjson_php_alc, &err);
    if (doc == NULL && err.msg
            && strcmp(err.msg, "number is infinity when parsed as double") == 0
            && !fastjson_input_has_inf_nan_literal(json, json_len, false)) {
        /* Match ext/json: exponent-overflow numbers like "1e309" are
         * valid JSON values (decoded to INF). The pre-scan rejects
         * inputs that also contain an unquoted Inf/NaN literal, which
         * yyjson's flag would accept but ext/json doesn't. */
        doc = yyjson_read_opts(json, json_len,
                               yflags | YYJSON_READ_ALLOW_INF_AND_NAN,
                               &fastjson_php_alc, &err);
    }

    if (doc == NULL) {
        /* Match fastjson_decode: a malformed UTF-8 byte at the parse-
         * error position is JSON_ERROR_UTF8, not JSON_ERROR_SYNTAX.
         * A non-ASCII byte that starts a valid UTF-8 sequence (e.g.
         * a bare `é` at top level) is a syntactic JSON error, not a
         * UTF-8 error -- ext/json keeps SYNTAX for those. */
        if (err.pos < json_len
                && (unsigned char)json[err.pos] >= 0x80
                && !fastjson_byte_is_valid_utf8_start(json, json_len, err.pos)) {
            err.code = YYJSON_READ_ERROR_INVALID_STRING;
        }
        fastjson_set_read_error(json, json_len, &err);
        RETURN_FALSE;
    }

    yyjson_doc_free(doc);
    fastjson_clear_error();
    RETURN_TRUE;
}

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

PHP_FUNCTION(fastjson_last_error_info)
{
    ZEND_PARSE_PARAMETERS_NONE();

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
}

PHP_RINIT_FUNCTION(fastjson)
{
    /* Reset on every request -- module globals persist across requests
     * in non-ZTS builds, so a previous request's error state would
     * otherwise bleed into the next. */
    FASTJSON_G(last_err_code) = FASTJSON_ERROR_NONE;
    FASTJSON_G(last_err_msg) = NULL;
    FASTJSON_G(last_err_pos) = -1;
    FASTJSON_G(last_err_line) = 0;
    FASTJSON_G(last_err_col) = 0;
    return SUCCESS;
}

PHP_MINIT_FUNCTION(fastjson)
{
    register_fastjson_symbols(module_number);

    /* ext/json is a standard built-in module; it loads before fastjson
     * because we don't declare a dependency. The classes are registered
     * by ext/json's MINIT, which has already run by the time we get
     * here. zend_hash_str_find_ptr walks the class table case-folded to
     * lowercase, so query with the lowercase name. */
    fastjson_json_exception_ce = zend_hash_str_find_ptr(CG(class_table),
        "jsonexception", sizeof("jsonexception") - 1);
    fastjson_json_serializable_ce = zend_hash_str_find_ptr(CG(class_table),
        "jsonserializable", sizeof("jsonserializable") - 1);

    return SUCCESS;
}

PHP_MSHUTDOWN_FUNCTION(fastjson)
{
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

/* Declare ext/json as an OPTIONAL dependency. This is purely a
 * load-ordering guarantee: when ext/json is present (every standard
 * PHP build), the engine runs its MINIT before ours, so the
 * JsonException / JsonSerializable lookups in PHP_MINIT_FUNCTION below
 * reliably resolve out of CG(class_table) regardless of static-vs-shared
 * build or registration order. OPTIONAL (not REQUIRED) deliberately
 * preserves the documented degrade-gracefully behavior when ext/json is
 * somehow absent: the class-entry pointers stay NULL and fastjson still
 * loads (throwing \Exception, ignoring JsonSerializable). */
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
    NULL,
    PHP_MINFO(fastjson),
    PHP_FASTJSON_VERSION,
    PHP_MODULE_GLOBALS(fastjson),
    PHP_GINIT(fastjson),
    NULL,
    NULL,
    STANDARD_MODULE_PROPERTIES_EX
};

#ifdef COMPILE_DL_FASTJSON
#ifdef ZTS
ZEND_TSRMLS_CACHE_DEFINE()
#endif
ZEND_GET_MODULE(fastjson)
#endif
