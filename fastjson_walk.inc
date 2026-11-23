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
 * Template body for the recursive immutable yyjson_val -> zval decode walker.
 * #included with FJ_WALK_NAME set to the function name and FJ_WALK_SANITIZE
 * set to a literal 0 or 1 so the fast (0) instantiation omits sanitize-only
 * code.
 *
 * Ownership / depth notes:
 *  - Every refcounted zval written into `out` is owned by `out`; callers
 *    transfer it into a bucket via zend_hash_*_update or into return_value.
 *  - On failure `out` may be a partial array/object; the caller's
 *    zval_ptr_dtor frees partially built children exactly once.
 *  - remaining_depth follows ext/json's json_decode convention: a top-level
 *    scalar at depth 1 is allowed; a container needs depth >= 2.
 */

static zend_always_inline bool FJ_WALK_STR_NAME(yyjson_val *val,
                                                zend_long flags, zval *out)
{
    const char *raw = yyjson_get_str(val);
    size_t raw_len = yyjson_get_len(val);
#if FJ_WALK_SANITIZE
    if (UNEXPECTED(yyjson_get_tag(val) & YYJSON_RESERVED_INVALID_UNICODE)) {
        zend_string *zs = fastjson_sanitize_utf8_zstr(raw, raw_len, flags,
                                                      FJ_SAN_DECODE);
        if (UNEXPECTED(zs == NULL)) {
            ZVAL_NULL(out);
            return false;
        }
        ZVAL_STR(out, zs);
        return true;
    }
#else
    (void)flags;
#endif
    /* Avoid ZVAL_STRINGL_FAST: PHP 8.4.28+ routes zend_string_init_fast()
     * through zend_string_finit() -> _safe_emalloc(len, 1, 32), which adds
     * +8 B to 7/8 of allocated strings compared to zend_string_init(). */
    if (EXPECTED(raw_len > 1)) {
        ZVAL_STRINGL(out, raw, raw_len);
    } else if (raw_len == 0) {
        ZVAL_EMPTY_STRING(out);
    } else {
        ZVAL_INTERNED_STR(out, ZSTR_CHAR((unsigned char)raw[0]));
    }
    return true;
}

static zend_always_inline bool FJ_WALK_LEAF_NAME(const fj_walk_ctx *wctx,
                                                 yyjson_val *val,
                                                 zend_long remaining_depth,
                                                 zval *out)
{
    switch (yyjson_get_type(val)) {
    case YYJSON_TYPE_NULL:
        ZVAL_NULL(out);
        return true;
    case YYJSON_TYPE_BOOL:
        ZVAL_BOOL(out, yyjson_get_bool(val));
        return true;
    case YYJSON_TYPE_NUM:
        fj_num_to_zval(val, wctx->flags, out);
        return true;
    case YYJSON_TYPE_STR:
        return FJ_WALK_STR_NAME(val, wctx->flags, out);
    case YYJSON_TYPE_RAW:
        fj_raw_to_zval(val, out);
        return true;
    default:
        return FJ_WALK_NAME(wctx, val, remaining_depth, out);
    }
}

static bool FJ_WALK_NAME(const fj_walk_ctx *wctx, yyjson_val *val,
                         zend_long remaining_depth, zval *out)
{
    if (UNEXPECTED(remaining_depth <= 0)) {
        fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                "Maximum stack depth exceeded");
        ZVAL_NULL(out);
        return false;
    }

    yyjson_type t = yyjson_get_type(val);

    switch (t) {
    case YYJSON_TYPE_NULL:
        ZVAL_NULL(out);
        return true;

    case YYJSON_TYPE_BOOL:
        ZVAL_BOOL(out, yyjson_get_bool(val));
        return true;

    case YYJSON_TYPE_NUM:
        fj_num_to_zval(val, wctx->flags, out);
        return true;

    case YYJSON_TYPE_RAW:
        fj_raw_to_zval(val, out);
        return true;

    case YYJSON_TYPE_STR:
        return FJ_WALK_STR_NAME(val, wctx->flags, out);

    case YYJSON_TYPE_ARR: {
        if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
            fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                    "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        if (UNEXPECTED(remaining_depth <= 1)) {
            fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                    "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = yyjson_arr_size(val);
        if (n == 0) {
            ZVAL_EMPTY_ARRAY(out);
            return true;
        }
        array_init_size(out, (uint32_t)n);
        zend_hash_real_init_packed(Z_ARRVAL_P(out));
        yyjson_val *item;
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(val, &iter);
        zend_long child_depth = remaining_depth - 1;
        bool fill_ok = true;
        ZEND_HASH_FILL_PACKED(Z_ARRVAL_P(out)) {
            while ((item = yyjson_arr_iter_next(&iter))) {
                if (UNEXPECTED(fastjson_oom_pending())) {
                    fill_ok = false;
                    break;
                }
                zval child;
                if (UNEXPECTED(!FJ_WALK_LEAF_NAME(wctx, item, child_depth,
                                                  &child))) {
                    zval_ptr_dtor(&child);
                    fill_ok = false;
                    break;
                }
                ZEND_HASH_FILL_ADD(&child);
            }
        } ZEND_HASH_FILL_END();
        return fill_ok;
    }

    case YYJSON_TYPE_OBJ: {
        if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
            fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                    "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        if (UNEXPECTED(remaining_depth <= 1)) {
            fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                    "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = yyjson_obj_size(val);
        bool assoc = wctx->assoc;
        if (n == 0) {
            if (assoc) {
                ZVAL_EMPTY_ARRAY(out);
            } else {
                object_init(out);
            }
            return true;
        }
        HashTable *ht;
        if (assoc) {
            array_init_size(out, (uint32_t)n);
            ht = Z_ARRVAL_P(out);
        } else {
            object_init(out);
            ht = zend_new_array((uint32_t)n);
            zend_hash_real_init_mixed(ht);
            Z_OBJ_P(out)->properties = ht;
        }
        /* Skip key sharing on wide map-like objects (n > 64) whose keys are
         * typically unique; avoids polluting the cache and saves probe work. */
        fj_key_cache *obj_share = (n <= 64 && wctx->share != NULL
                                   && !wctx->share->off)
            ? wctx->share : NULL;
        zend_long child_depth = remaining_depth - 1;
        yyjson_val *key;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(val, &iter);
        while ((key = yyjson_obj_iter_next(&iter))) {
            if (UNEXPECTED(fastjson_oom_pending())) {
                return false;
            }
            yyjson_val *vval = yyjson_obj_iter_get_val(key);
            const char *kstr = yyjson_get_str(key);
            size_t klen = yyjson_get_len(key);
            const char *use_key = kstr;
            size_t use_klen = klen;
#if FJ_WALK_SANITIZE
            bool key_owned = false;
            if (UNEXPECTED(yyjson_get_tag(key) & YYJSON_RESERVED_INVALID_UNICODE)) {
                use_key = fastjson_sanitize_utf8(kstr, klen, wctx->flags,
                                                 FJ_SAN_DECODE, &use_klen);
                if (UNEXPECTED(fastjson_oom_pending())) {
                    efree((char *)use_key);
                    return false;
                }
                key_owned = true;
            }
#endif
            if (!assoc && UNEXPECTED(use_klen > 0 && use_key[0] == '\0')) {
#if FJ_WALK_SANITIZE
                if (key_owned) efree((char *)use_key);
#endif
                fastjson_set_error_code(FASTJSON_ERROR_INVALID_PROPERTY_NAME,
                    "The decoded property name is invalid");
                return false;
            }
            zval child;
            if (UNEXPECTED(!FJ_WALK_LEAF_NAME(wctx, vval, child_depth,
                                              &child))) {
#if FJ_WALK_SANITIZE
                if (key_owned) efree((char *)use_key);
#endif
                zval_ptr_dtor(&child);
                return false;
            }
            fj_insert_obj_member(ht, obj_share, assoc, use_key, use_klen,
                                 &child);
#if FJ_WALK_SANITIZE
            if (key_owned) efree((char *)use_key);
#endif
        }
        return true;
    }

    default:
        ZVAL_NULL(out);
        return true;
    }
}
