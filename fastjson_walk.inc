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
 * #included twice from fastjson_decode.c with FJ_WALK_NAME set to the
 * function name and FJ_WALK_SANITIZE set to a literal 0 or 1. Because
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
 *        that isn't already well-formed UTF-8 (decode-side semantics:
 *        SUBSTITUTE wins on BOTH-bits, per-byte advance on invalid);
 *        a well-formed string still takes the borrowed pointer, so a
 *        clean payload pays only the well-formed scan.
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
static bool FJ_WALK_NAME(yyjson_val *val, bool assoc,
                         zend_long remaining_depth,
                         zend_long flags, zval *out)
{
    if (remaining_depth <= 0) {
        fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
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
        fj_num_to_zval(val, flags, out);
        return true;

    case YYJSON_TYPE_RAW:
        fj_raw_to_zval(val, out);
        return true;

    case YYJSON_TYPE_STR: {
        const char *raw = yyjson_get_str(val);
        size_t raw_len = yyjson_get_len(val);
#if FJ_WALK_SANITIZE
        /* Defensive callers often set IGNORE / SUBSTITUTE on every decode
         * even when the payload is clean UTF-8; scan first so a
         * well-formed string skips the sanitize alloc + copy. */
        if (!fastjson_utf8_well_formed(raw, raw_len)) {
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
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        if (remaining_depth <= 1) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = yyjson_arr_size(val);
        array_init_size(out, (uint32_t)n);
        yyjson_val *item;
        yyjson_arr_iter iter;
        yyjson_arr_iter_init(val, &iter);
        while ((item = yyjson_arr_iter_next(&iter))) {
            zval child;
            if (!FJ_WALK_NAME(item, assoc, remaining_depth - 1, flags,
                              &child)) {
                zval_ptr_dtor(&child);
                return false;
            }
            zend_hash_next_index_insert(Z_ARRVAL_P(out), &child);
        }
        return true;
    }

    case YYJSON_TYPE_OBJ: {
        if (UNEXPECTED(zend_call_stack_overflowed(EG(stack_limit)))) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        if (remaining_depth <= 1) {
            fastjson_set_encode_error(FASTJSON_ERROR_DEPTH,
                                      "Maximum stack depth exceeded");
            ZVAL_NULL(out);
            return false;
        }
        size_t n = yyjson_obj_size(val);
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
        }
        yyjson_val *key;
        yyjson_obj_iter iter;
        yyjson_obj_iter_init(val, &iter);
        while ((key = yyjson_obj_iter_next(&iter))) {
            yyjson_val *vval = yyjson_obj_iter_get_val(key);
            const char *kstr = yyjson_get_str(key);
            size_t klen = yyjson_get_len(key);
            const char *use_key = kstr;
            size_t use_klen = klen;
#if FJ_WALK_SANITIZE
            /* Same fast path as the string-value case: a well-formed key
             * uses yyjson's borrowed pointer; only a malformed key is
             * rewritten (owned, efree'd below). */
            bool key_owned = false;
            if (!fastjson_utf8_well_formed(kstr, klen)) {
                use_key = fastjson_sanitize_utf8(kstr, klen, flags,
                                                 FJ_SAN_DECODE, &use_klen);
                key_owned = true;
            }
#endif
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
