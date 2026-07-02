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
 * fastjson_encode entry point. The actual encoder lives in
 * fastjson_directwrite.c (one-stage walker emitting straight into a
 * smart_str buffer; see that file's header for the architecture
 * rationale). This file owns just the ZPP, the JSON_THROW_ON_ERROR
 * state-preservation contract, and the fail-path dispatch (false vs
 * \JsonException).
 *
 * History note: this used to host a two-stage encoder that walked
 * zvals into a yyjson_mut_doc, then called yyjson_mut_write_opts on
 * the doc. That approach hit a structural ceiling on workloads with
 * many small values (citm_catalog, random, truenull) where the
 * per-value mut_val allocation cost wasn't recovered by yyjson's
 * faster writer. The direct-write rewrite closed that gap (encode
 * aggregate +77%, peak memory -5.3x, citm_catalog flipped from 0.65x
 * to 1.29x vs ext/json).
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

    /* JSON_THROW_ON_ERROR contract: on error, throw \JsonException
     * and leave the global fastjson_last_error state unchanged
     * (json_exceptions_error_clearing.phpt). Snapshot at entry,
     * restore on the throw path. Non-throw path follows ext/json:
     * clear at entry, errors during encode (including PARTIAL_OUTPUT
     * substitutions) persist for fastjson_last_error() to surface. */
    bool throw_mode = (flags & FASTJSON_ENCODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);
    if (!throw_mode) {
        fastjson_clear_error();
    }

    zend_string *zs = fastjson_directwrite_encode(value, flags, depth);
    if (zs == NULL) {
        goto fail;
    }

    RETURN_NEW_STR(zs);

fail:
    /* If a user-level exception is already pending (e.g. from a
     * jsonSerialize() callback), don't paper over it with our own. */
    if (EG(exception)) {
        RETURN_THROWS();
    }
    if (throw_mode) {
        /* Throw \JsonException when ext/json provided it (always in
         * standard builds); fall back to \Exception otherwise. */
        zend_class_entry *ce = fastjson_json_exception_ce
            ? fastjson_json_exception_ce : zend_ce_exception;
        zend_long throw_code = FASTJSON_G(last_err_code);
        const char *throw_msg = FASTJSON_G(last_err_msg)
            ? FASTJSON_G(last_err_msg) : "fastjson_encode failed";
        /* Restore prior global state: THROW_ON_ERROR's contract is
         * "exception carries the error, global state untouched". */
        fastjson_restore_error_state(&saved_err);
        zend_throw_exception(ce, throw_msg, throw_code);
        RETURN_THROWS();
    }
    RETURN_FALSE;
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

    /* Same THROW_ON_ERROR state-preservation contract as
     * fastjson_encode: snapshot at entry, restore on the throw path. */
    bool throw_mode = (flags & FASTJSON_ENCODE_THROW_ON_ERROR) != 0;
    fastjson_error_state saved_err;
    fastjson_save_error_state(&saved_err);
    if (!throw_mode) {
        fastjson_clear_error();
    }

    /* Encode first; only touch the filesystem once we have bytes to
     * write. Reuses the exact encoder + error semantics of
     * fastjson_encode. */
    zend_string *zs = fastjson_directwrite_encode(value, flags, depth);
    if (zs == NULL) {
        goto encode_fail;
    }

    /* Write through the streams layer (wrappers + open_basedir honored).
     * Silent: no REPORT_ERRORS. An I/O failure is not a JSON error, so
     * it never throws even under JSON_THROW_ON_ERROR -- the false return
     * is unambiguous; we set last_error only for introspection. */
    php_stream_context *context = php_stream_context_from_zval(NULL, 0);
    php_stream *stream = php_stream_open_wrapper_ex(path, "wb", 0, NULL,
                                                    context);
    if (stream == NULL) {
        zend_string_release(zs);
        /* A userspace stream wrapper's open() may have thrown; propagate
         * that rather than masking it (and never RETURN_FALSE with an
         * exception still pending). */
        if (EG(exception)) {
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_encode_error(FASTJSON_ERROR_SYNTAX,
                                  "Failed to open file for writing");
        RETURN_FALSE;
    }
    size_t written = php_stream_write(stream, ZSTR_VAL(zs), ZSTR_LEN(zs));
    php_stream_close(stream);
    bool wrote_all = (written == ZSTR_LEN(zs));
    zend_string_release(zs);
    if (!wrote_all) {
        /* A userspace wrapper's write() may have thrown mid-write. */
        if (EG(exception)) {
            fastjson_restore_error_state(&saved_err);
            RETURN_THROWS();
        }
        fastjson_set_encode_error(FASTJSON_ERROR_SYNTAX,
                                  "Failed to write file");
        RETURN_FALSE;
    }
    RETURN_TRUE;

encode_fail:
    /* If a user-level exception is already pending (e.g. from a
     * jsonSerialize() callback), don't paper over it with our own. */
    if (EG(exception)) {
        RETURN_THROWS();
    }
    if (throw_mode) {
        zend_class_entry *ce = fastjson_json_exception_ce
            ? fastjson_json_exception_ce : zend_ce_exception;
        zend_long throw_code = FASTJSON_G(last_err_code);
        const char *throw_msg = FASTJSON_G(last_err_msg)
            ? FASTJSON_G(last_err_msg) : "fastjson_file_encode failed";
        fastjson_restore_error_state(&saved_err);
        zend_throw_exception(ce, throw_msg, throw_code);
        RETURN_THROWS();
    }
    RETURN_FALSE;
}
