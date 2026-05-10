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
    zend_long saved_err_code = FASTJSON_G(last_err_code);
    const char *saved_err_msg = FASTJSON_G(last_err_msg);
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
        FASTJSON_G(last_err_code) = saved_err_code;
        FASTJSON_G(last_err_msg) = saved_err_msg;
        zend_throw_exception(ce, throw_msg, throw_code);
        RETURN_THROWS();
    }
    RETURN_FALSE;
}
