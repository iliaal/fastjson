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
    /* Publish this invocation's outcome so nested userland encodes cannot
     * leak their error state: non-throw reports the invocation result,
     * throw-mode restores the entry state on success (failures throw with
     * the entry state restored by fastjson_throw_error). */
    fastjson_restore_error_state(throw_mode ? saved_err : encode_err);
    if (zs != NULL) {
        return zs;
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

    /* Match ext/json: validate depth at the first container, without ValueError. */

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

    /* Omit REPORT_ERRORS; open_basedir warnings still come from the wrapper.
     * I/O failures return false and set last_error without a JsonException. */
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
    /* php_stream_write may short-write (userspace wrappers report any
     * count), so loop to completion; a <= 0 return means no progress. */
    size_t total = ZSTR_LEN(zs);
    size_t done = 0;
    while (done < total) {
        ssize_t n = php_stream_write(stream, ZSTR_VAL(zs) + done,
                                     total - done);
        if (n <= 0) {
            break;
        }
        if ((size_t)n >= total - done) {
            done = total;
            break;
        }
        done += (size_t)n;
    }
    int close_res = php_stream_close(stream);
    bool wrote_all = (done == total);
    zend_string_release(zs);
    if (EG(exception)) {
        fastjson_restore_error_state(throw_mode ? &saved_err : &encode_err);
        RETURN_THROWS();
    }
    if (!wrote_all || close_res != 0) {
        fastjson_set_error_code(FASTJSON_ERROR_SYNTAX,
                                "Failed to write file");
        RETURN_FALSE;
    }
    fastjson_restore_error_state(throw_mode ? &saved_err : &encode_err);
    RETURN_TRUE;
}
