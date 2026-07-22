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
 * Template body for the recursive yyjson_val -> zval decode walker.
 * #included for immutable and mutable yyjson values with FJ_WALK_NAME set
 * to the function name, FJ_WALK_MUTABLE selecting the representation, and
 * FJ_WALK_SANITIZE set to a literal 0 or 1. Because
 * FJ_WALK_SANITIZE is a preprocessor constant, the sanitize-only code is
 * physically absent from the fast (0) instantiation -- the two stamped
 * functions are as branch-free and specialized as hand-written copies,
 * but the container / number / depth scaffolding lives in exactly one
 * place. Any change here applies to both walkers by construction, which
 * is the whole point of the shared template.
 *
 * FJ_WALK_SANITIZE selects the string/key emit strategy only:
 *   0 -> yyjson's bytes go straight into the zval (fast path).
 *   1 -> fastjson_sanitize_utf8 runs on any string value or object key
 *        tagged by yyjson's P-005 parser hook as containing malformed
 *        UTF-8 (decode-side semantics: SUBSTITUTE wins on BOTH-bits,
 *        per-byte advance on invalid). Clean strings take the borrowed
 *        pointer without a second validation scan.
 *
 * Ownership / depth notes (identical for both instantiations):
 *  - Every refcounted zval written into `out` is owned by `out`; callers
 *    transfer it into a bucket via zend_hash_*_insert/update (copies the
 *    zval bytes, refcount unchanged) or into return_value via RETVAL_*.
 *  - On failure `out` may be a partial array/object; the caller's
 *    zval_ptr_dtor frees the partially built children exactly once.
 *  - remaining_depth follows ext/json's json_decode convention: it
 *    counts every level (scalar leaves included), decremented before
 *    each recurse. A top-level scalar at depth 1 is allowed; a container
 *    needs depth >= 2 (json_decode("[]", true, 1) -> NULL).
 */
#if FJ_WALK_MUTABLE
# define FJ_WALK_VAL yyjson_mut_val
# define FJ_GET_TYPE yyjson_mut_get_type
# define FJ_GET_BOOL yyjson_mut_get_bool
# define FJ_GET_STR yyjson_mut_get_str
# define FJ_GET_LEN yyjson_mut_get_len
# define FJ_GET_TAG yyjson_mut_get_tag
# define FJ_ARR_SIZE yyjson_mut_arr_size
# define FJ_ARR_ITER yyjson_mut_arr_iter
# define FJ_ARR_ITER_INIT yyjson_mut_arr_iter_init
# define FJ_ARR_ITER_NEXT yyjson_mut_arr_iter_next
# define FJ_OBJ_SIZE yyjson_mut_obj_size
# define FJ_OBJ_ITER yyjson_mut_obj_iter
# define FJ_OBJ_ITER_INIT yyjson_mut_obj_iter_init
# define FJ_OBJ_ITER_NEXT yyjson_mut_obj_iter_next
# define FJ_OBJ_ITER_VAL yyjson_mut_obj_iter_get_val
# define FJ_NUM_TO_ZVAL fj_mut_num_to_zval
# define FJ_RAW_TO_ZVAL fj_mut_raw_to_zval
#else
# define FJ_WALK_VAL yyjson_val
# define FJ_GET_TYPE yyjson_get_type
# define FJ_GET_BOOL yyjson_get_bool
# define FJ_GET_STR yyjson_get_str
# define FJ_GET_LEN yyjson_get_len
# define FJ_GET_TAG yyjson_get_tag
# define FJ_ARR_SIZE yyjson_arr_size
# define FJ_ARR_ITER yyjson_arr_iter
# define FJ_ARR_ITER_INIT yyjson_arr_iter_init
# define FJ_ARR_ITER_NEXT yyjson_arr_iter_next
# define FJ_OBJ_SIZE yyjson_obj_size
# define FJ_OBJ_ITER yyjson_obj_iter
# define FJ_OBJ_ITER_INIT yyjson_obj_iter_init
# define FJ_OBJ_ITER_NEXT yyjson_obj_iter_next
# define FJ_OBJ_ITER_VAL yyjson_obj_iter_get_val
# define FJ_NUM_TO_ZVAL fj_num_to_zval
# define FJ_RAW_TO_ZVAL fj_raw_to_zval
#endif

static bool FJ_WALK_NAME(FJ_WALK_VAL *val, bool assoc,
                         zend_long remaining_depth,
                         zend_long flags, zval *out)
{
    if (remaining_depth <= 0) {
        fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                "Maximum stack depth exceeded");
        ZVAL_NULL(out);
        return false;
    }

    yyjson_type t = FJ_GET_TYPE(val);

    switch (t) {
    case YYJSON_TYPE_NULL:
        ZVAL_NULL(out);
        return true;

    case YYJSON_TYPE_BOOL:
        ZVAL_BOOL(out, FJ_GET_BOOL(val));
        return true;

    case YYJSON_TYPE_NUM:
        FJ_NUM_TO_ZVAL(val, flags, out);
        return true;

    case YYJSON_TYPE_RAW:
        FJ_RAW_TO_ZVAL(val, out);
        return true;

    case YYJSON_TYPE_STR: {
        const char *raw = FJ_GET_STR(val);
        size_t raw_len = FJ_GET_LEN(val);
#if FJ_WALK_SANITIZE
        if (FJ_GET_TAG(val) & YYJSON_RESERVED_INVALID_UNICODE) {
            size_t sane_len;
            char *sane = fastjson_sanitize_utf8(raw, raw_len, flags,
                                                FJ_SAN_DECODE, &sane_len);
            ZVAL_STRINGL(out, sane, sane_len);
            efree(sane);
            return true;
        }
#endif
        /* yyjson_get_len is the byte length; ZVAL_STRINGL preserves
         * embedded NULs that may appear after a \0 escape. */
        ZVAL_STRINGL(out, raw, raw_len);
        return true;
    }

    case YYJSON_TYPE_ARR: {
        /* Stack-overflow guard for runaway recursion (parallel to the
         * encoder's check; fires when remaining_depth is configured
         * looser than zend.max_allowed_stack_size allows). */
        if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
            fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                    "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        if (remaining_depth <= 1) {
            fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                    "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = FJ_ARR_SIZE(val);
        array_init_size(out, (uint32_t)n);
        if (n == 0) {
            return true;
        }
        zend_hash_real_init_packed(Z_ARRVAL_P(out));
        FJ_WALK_VAL *item;
        FJ_ARR_ITER iter;
        FJ_ARR_ITER_INIT(val, &iter);
        bool fill_ok = true;
        ZEND_HASH_FILL_PACKED(Z_ARRVAL_P(out)) {
            while ((item = FJ_ARR_ITER_NEXT(&iter))) {
                zval child;
                if (!FJ_WALK_NAME(item, assoc, remaining_depth - 1, flags,
                                  &child)) {
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
        if (remaining_depth <= 1) {
            fastjson_set_error_code(FASTJSON_ERROR_DEPTH,
                                    "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = FJ_OBJ_SIZE(val);
        HashTable *ht;
        if (assoc) {
            array_init_size(out, (uint32_t)n);
            ht = Z_ARRVAL_P(out);
        } else {
            object_init(out);
            /* Z_OBJPROP_P triggers lazy properties HashTable creation
             * on the freshly-init'd stdClass. zend_hash_update on it
             * directly avoids the write_property handler dispatch +
             * zval copy that add_property_zval_ex would impose. */
            ht = Z_OBJPROP_P(out);
            if (n != 0) {
                zend_hash_extend(ht, (uint32_t)n, false);
            }
        }
        FJ_WALK_VAL *key;
        FJ_OBJ_ITER iter;
        FJ_OBJ_ITER_INIT(val, &iter);
        while ((key = FJ_OBJ_ITER_NEXT(&iter))) {
            FJ_WALK_VAL *vval = FJ_OBJ_ITER_VAL(key);
            const char *kstr = FJ_GET_STR(key);
            size_t klen = FJ_GET_LEN(key);
            const char *use_key = kstr;
            size_t use_klen = klen;
#if FJ_WALK_SANITIZE
            bool key_owned = false;
            if (FJ_GET_TAG(key) & YYJSON_RESERVED_INVALID_UNICODE) {
                use_key = fastjson_sanitize_utf8(kstr, klen, flags,
                                                 FJ_SAN_DECODE, &use_klen);
                key_owned = true;
            }
#endif
            if (!assoc && use_klen > 0 && use_key[0] == '\0') {
#if FJ_WALK_SANITIZE
                if (key_owned) efree((char *)use_key);
#endif
                fastjson_set_error_code(FASTJSON_ERROR_INVALID_PROPERTY_NAME,
                    "The decoded property name is invalid");
                return false;
            }
            zval child;
            if (!FJ_WALK_NAME(vval, assoc, remaining_depth - 1, flags,
                              &child)) {
#if FJ_WALK_SANITIZE
                if (key_owned) efree((char *)use_key);
#endif
                zval_ptr_dtor(&child);
                return false;
            }
            if (assoc) {
                /* zend_symtable_str_update applies PHP's standard
                 * "0" -> 0 numeric-string-to-int promotion for array
                 * keys. Empty key is allowed and stored as "". */
                zend_symtable_str_update(ht, use_key, use_klen, &child);
            } else {
                /* Object properties keep string keys verbatim; "0"
                 * stays "0" so $obj->{"0"} works as expected. */
                zend_string *zk = zend_string_init(use_key, use_klen, 0);
                zend_hash_update(ht, zk, &child);
                zend_string_release_ex(zk, 0);
            }
#if FJ_WALK_SANITIZE
            if (key_owned) efree((char *)use_key);
#endif
        }
        return true;
    }

    default:
        /* YYJSON_TYPE_NONE shouldn't reach a parsed doc with default
         * flags. Fall back to null rather than asserting. */
        ZVAL_NULL(out);
        return true;
    }
}

#undef FJ_WALK_VAL
#undef FJ_GET_TYPE
#undef FJ_GET_BOOL
#undef FJ_GET_STR
#undef FJ_GET_LEN
#undef FJ_GET_TAG
#undef FJ_ARR_SIZE
#undef FJ_ARR_ITER
#undef FJ_ARR_ITER_INIT
#undef FJ_ARR_ITER_NEXT
#undef FJ_OBJ_SIZE
#undef FJ_OBJ_ITER
#undef FJ_OBJ_ITER_INIT
#undef FJ_OBJ_ITER_NEXT
#undef FJ_OBJ_ITER_VAL
#undef FJ_NUM_TO_ZVAL
#undef FJ_RAW_TO_ZVAL
