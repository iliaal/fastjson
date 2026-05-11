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
#include "ext/standard/info.h"
#include "php_fastjson.h"
#include "fastjson_arginfo.h"
#include "yyjson.h"
#include "fastjson_alloc.h"

ZEND_DECLARE_MODULE_GLOBALS(fastjson)

zend_class_entry *fastjson_json_exception_ce = NULL;
zend_class_entry *fastjson_json_serializable_ce = NULL;

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
}

void fastjson_clear_error(void)
{
    FASTJSON_G(last_err_code) = FASTJSON_ERROR_NONE;
    FASTJSON_G(last_err_msg) = NULL;
}

void fastjson_set_encode_error(zend_long code, const char *msg)
{
    FASTJSON_G(last_err_code) = code;
    FASTJSON_G(last_err_msg) = msg;
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
    bool has_ignore = (flags & (1L << 20)) != 0;
    bool has_subst  = (flags & (1L << 21)) != 0;
    bool substitute = (mode == FJ_SAN_DECODE)
        ? has_subst                   /* decode: any SUBSTITUTE wins */
        : (has_subst && !has_ignore); /* encode: BOTH -> IGNORE strips */

    /* Worst-case output: 3 bytes per input byte (each invalid byte
     * substituted by the 3-byte U+FFFD). IGNORE-only is <= input-sized. */
    size_t cap = substitute ? len * 3 + 1 : (len > 0 ? len : 1);
    char *out = emalloc(cap);
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

bool fastjson_input_has_inf_nan_literal(const char *s, size_t len)
{
    /* Walk outside string literals. Inside "..." backslash escapes
     * the next char. Anywhere unquoted, an ASCII run starting with
     * I/i + N/n + F/f (Inf or Infinity) or N/n + A/a + N/n (NaN) is
     * the literal yyjson's ALLOW_INF_AND_NAN accepts and ext/json
     * doesn't. yyjson also accepts a leading '-'; we still match
     * either form via the I/N start. */
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
        fastjson_set_error(YYJSON_READ_ERROR_INVALID_PARAMETER,
                           "input length is 0");
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
            && !fastjson_input_has_inf_nan_literal(json, json_len)) {
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
        fastjson_set_error(err.code, err.msg);
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

PHP_GINIT_FUNCTION(fastjson)
{
#if defined(COMPILE_DL_FASTJSON) && defined(ZTS)
    ZEND_TSRMLS_CACHE_UPDATE();
#endif
    fastjson_globals->last_err_code = FASTJSON_ERROR_NONE;
    fastjson_globals->last_err_msg = NULL;
}

PHP_RINIT_FUNCTION(fastjson)
{
    /* Reset on every request -- module globals persist across requests
     * in non-ZTS builds, so a previous request's error state would
     * otherwise bleed into the next. */
    FASTJSON_G(last_err_code) = FASTJSON_ERROR_NONE;
    FASTJSON_G(last_err_msg) = NULL;
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

zend_module_entry fastjson_module_entry = {
    STANDARD_MODULE_HEADER,
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
