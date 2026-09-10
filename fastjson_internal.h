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

/* Include through php_fastjson.h for the required PHP and yyjson types. */

#ifndef PHP_FASTJSON_INTERNAL_H
#define PHP_FASTJSON_INTERNAL_H

#include "php_streams.h"

typedef enum {
    FJ_STRING_SIZE_OK = 0,
    FJ_STRING_SIZE_INVALID_UTF8,
    FJ_STRING_SIZE_TOO_LARGE,
} fj_string_size_status;

/* Compute yyjson_write_string_to_buf()'s exact output length, including
 * quotes, while validating UTF-8. Used only for large strings so the writer
 * does not reserve its 6x worst case when the actual output is much smaller. */
fj_string_size_status fastjson_json_string_size(const char *s, size_t len,
                                                 yyjson_write_flag flags,
                                                 size_t *out_len,
                                                 bool *copyable_ascii);

/* Append one large JSON string to `buf` with exact capacity. On OK the
 * smart-string length is advanced; other statuses leave it unchanged. */
fj_string_size_status fastjson_write_large_json_string(
    smart_str *buf, const char *s, size_t len, yyjson_write_flag flags);
bool fastjson_apply_hex_escapes(smart_str *buf, zend_long flags,
                                size_t start_pos);

#define FASTJSON_HAS_UTF8_HANDLING_FLAG(flags) \
    (((flags) & (FASTJSON_INVALID_UTF8_IGNORE \
                 | FASTJSON_INVALID_UTF8_SUBSTITUTE)) != 0)

/* Avoid yyjson's 6x reservation: at 256 KiB, clean/late-escape ASCII measured
 * -35%/-31% on x86_64, +8% on aarch64; both gain ~40% by 512 KiB.
 * Re-measure both architectures before moving the threshold. */
#define FASTJSON_EXACT_STRING_THRESHOLD (256 * 1024)
/* Non-ASCII preflight measured +75%/+160% on x86_64/aarch64 at 1 MiB.
 * Delay it until the 6x reservation reaches 48 MiB against a 128M memory_limit.
 * Clean ASCII bypasses preflight via the fused scan-and-copy path. */
#define FASTJSON_EXACT_NONASCII_THRESHOLD (8 * 1024 * 1024)
#define FASTJSON_ENCODE_HEX_MASK (FASTJSON_ENCODE_HEX_TAG \
    | FASTJSON_ENCODE_HEX_AMP | FASTJSON_ENCODE_HEX_APOS \
    | FASTJSON_ENCODE_HEX_QUOT)
/* yyjson_write_number requires at least 21 bytes for integers, 40 for reals. */
#define FASTJSON_NUM_INT_WORST 24
#define FASTJSON_NUM_REAL_WORST 40

static zend_always_inline void fastjson_append_newline_indent(
    smart_str *buf, int level)
{
    if (UNEXPECTED(level > 8
            && (size_t)level <= (ZSTR_MAX_LEN - 1) / 4)) {
        size_t spaces = (size_t)level * 4;
        smart_str_alloc(buf, spaces + 1, 0);
        smart_str_appendc(buf, '\n');
        memset(ZSTR_VAL(buf->s) + ZSTR_LEN(buf->s), ' ', spaces);
        ZSTR_LEN(buf->s) += spaces;
        return;
    }
    smart_str_appendc(buf, '\n');
    for (int i = 0; i < level; i++) {
        smart_str_appendl(buf, "    ", 4);
    }
}

typedef enum {
    FJ_SPLICE_OK = 0,
    FJ_SPLICE_SETTABLE_FAIL,
    FJ_SPLICE_WRITE_FAIL,
    FJ_SPLICE_DEPTH_FAIL,
    FJ_SPLICE_TOO_LARGE,
    FJ_SPLICE_INF_OR_NAN,
    FJ_SPLICE_INVALID_UTF8,
    FJ_SPLICE_AMBIGUOUS,
} fj_splice_status;

typedef struct {
    const char *str;
    size_t len;
} fj_ptr_seg;

typedef struct {
    fj_ptr_seg *segs;
    char *storage;
    size_t nsegs;
} fastjson_pointer_plan;

typedef struct {
    char *owned_str;
    zend_string *encoded;
    yyjson_val stack_val;
    const yyjson_val *repl;
} fastjson_pointer_repl;

bool fastjson_pointer_build_replacement(zval *value, zend_long value_flags,
                                        zend_long depth,
                                        fastjson_pointer_repl *out,
                                        fastjson_error_state *error_state);

/* Count RFC 6901 segments in a pointer ("" -> 0, "/a" -> 1, "/a/b" -> 2).
 * The replacement in a non-root set nests under this many path containers,
 * so the caller subtracts it from the value's depth budget. */
size_t fastjson_pointer_count_segments(const char *ptr, size_t len);

bool fastjson_pointer_plan_init(yyjson_val *root, const char *pointer,
                                size_t pointer_len, size_t depth_limit,
                                fastjson_pointer_plan *plan,
                                fj_splice_status *status);
void fastjson_pointer_plan_destroy(fastjson_pointer_plan *plan);
fj_splice_status fastjson_pointer_resolve(yyjson_val *root,
                                          const char *pointer,
                                          size_t pointer_len,
                                          yyjson_val **target);

zend_string *fastjson_imut_pointer_set_write(yyjson_val *root,
                                             const fastjson_pointer_plan *plan,
                                             const fastjson_pointer_repl *replacement,
                                             zend_long flags,
                                             size_t depth_limit,
                                             fj_splice_status *status);

/* Shared PHP_FUNCTION entry prologue for the decode family, defined in
 * fastjson.c: snapshot the throw-mode error state, then validate $depth
 * exactly as ext/json does. Returns true on success; on $depth failure
 * raises ValueError and returns false, in which case the caller must
 * RETURN_THROWS(). */
bool fastjson_entry_prologue(zend_long flags, zend_long throw_bit,
                             zend_long depth, int depth_argno,
                             bool *throw_mode_out,
                             fastjson_error_state *saved_out);

#endif /* PHP_FASTJSON_INTERNAL_H */
