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
 * fastjson_encode / fastjson_file_encode entry points. The encoder itself
 * lives in fastjson_directwrite.c; this file owns ZPP, the
 * JSON_THROW_ON_ERROR state-preservation contract, fail-path dispatch
 * (false vs \JsonException), and the file-write tail for file_encode.
 */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "php.h"
#include "php_streams.h"
#include "ext/standard/file.h"
#include "Zend/zend_exceptions.h"
#include "php_fastjson.h"
#include "fastjson_arginfo.h"

/* Run direct-write encode and publish encode_err under the throw contract.
 * Returns the encoded string, or NULL on failure. When EG(exception) is
 * set (userland throw or JsonException in throw_mode), the caller must
 * RETURN_THROWS(); otherwise RETURN_FALSE. */
static zend_string *fastjson_do_encode(
    zval *value, zend_long flags, zend_long depth,
    bool throw_mode, const fastjson_error_state *saved_err,
    fastjson_error_state *encode_err, const char *throw_fallback)
{
    zend_string *zs = fastjson_directwrite_encode(value, flags, depth,
                                                   encode_err);
    if (UNEXPECTED(EG(exception))) {
        if (zs != NULL) {
            zend_string_release(zs);
        }
        if (!throw_mode) {
            fastjson_restore_error_state(encode_err);
        }
        return NULL;
    }
    if (!throw_mode) {
        /* Nested fastjson calls during callbacks must not replace this
         * invocation's final error state. */
        fastjson_restore_error_state(encode_err);
    }
    if (zs != NULL) {
        return zs;
    }
    if (EG(exception)) {
        return NULL;
    }
    if (throw_mode) {
        fastjson_throw_error(encode_err->code, encode_err->msg,
                             throw_fallback, saved_err);
    }
    return NULL;
}

PHP_FUNCTION(fastjson_encode)
{
    zval *value;
    zend_long flags = 0;
    zend_long depth = 512;

    ZEND_PARSE_PARAMETERS_START(1, 3)
        Z_PARAM_ZVAL(value)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(flags)
        Z_PARAM_LONG(depth)
    ZEND_PARSE_PARAMETERS_END();

    /* ext/json's json_encode does NOT validate $depth up front -- it
     * lets the recursion check inside the walker trip on the first
     * container when depth <= 0, returning false + JSON_ERROR_DEPTH.
     * That's how `json_encode($x, 0, 0)` returns false instead of
     * raising ValueError (bug81532.phpt). Match the contract. */

    bool throw_mode = (flags & FASTJSON_ENCODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_throw_mode_init(throw_mode, &saved_err);

    fastjson_error_state encode_err;
    zend_string *zs = fastjson_do_encode(value, flags, depth, throw_mode,
                                         &saved_err, &encode_err,
                                         "fastjson_encode failed");
    if (zs == NULL) {
        if (EG(exception)) {
            RETURN_THROWS();
        }
        RETURN_FALSE;
    }
    RETURN_STR(zs);
}

PHP_FUNCTION(fastjson_file_encode)
{
    char *path;
    size_t path_len;
    zval *value;
    zend_long flags = 0;
    zend_long depth = 512;

    ZEND_PARSE_PARAMETERS_START(2, 4)
        Z_PARAM_PATH(path, path_len)
        Z_PARAM_ZVAL(value)
        Z_PARAM_OPTIONAL
        Z_PARAM_LONG(flags)
        Z_PARAM_LONG(depth)
    ZEND_PARSE_PARAMETERS_END();

    bool throw_mode = (flags & FASTJSON_ENCODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_throw_mode_init(throw_mode, &saved_err);

    /* Encode first; only touch the filesystem once we have bytes to write. */
    fastjson_error_state encode_err;
    zend_string *zs = fastjson_do_encode(value, flags, depth, throw_mode,
                                         &saved_err, &encode_err,
                                         "fastjson_file_encode failed");
    if (zs == NULL) {
        if (EG(exception)) {
            RETURN_THROWS();
        }
        RETURN_FALSE;
    }

    /* Write through the streams layer (wrappers + open_basedir honored).
     * No REPORT_ERRORS, so a failed open/write emits no warning -- except
     * an open_basedir denial, whose warning the plain-file wrapper emits
     * regardless of REPORT_ERRORS (as file_put_contents does); we leave
     * that security-boundary warning visible. An I/O failure is not a JSON
     * error, so it never throws even under JSON_THROW_ON_ERROR -- the false
     * return is unambiguous; we set last_error only for introspection. */
    php_stream_context *context = php_stream_context_from_zval(NULL, 0);
    php_stream *stream = php_stream_open_wrapper_ex(path, "wb", 0, NULL,
                                                    context);
    if (stream == NULL) {
        zend_string_release(zs);
        /* A userspace stream wrapper's open() may have thrown; propagate
         * that rather than masking it (and never RETURN_FALSE with an
         * exception still pending). */
        if (EG(exception)) {
            fastjson_restore_error_state(throw_mode ? &saved_err : &encode_err);
            RETURN_THROWS();
        }
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "Failed to open file for writing");
        RETURN_FALSE;
    }
    size_t written = php_stream_write(stream, ZSTR_VAL(zs), ZSTR_LEN(zs));
    php_stream_close(stream);
    bool wrote_all = (written == ZSTR_LEN(zs));
    zend_string_release(zs);
    if (EG(exception)) {
        fastjson_restore_error_state(throw_mode ? &saved_err : &encode_err);
        RETURN_THROWS();
    }
    if (!wrote_all) {
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "Failed to write file");
        RETURN_FALSE;
    }
    if (!throw_mode) {
        fastjson_restore_error_state(&encode_err);
    }
    RETURN_TRUE;
}
