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

#ifndef FASTJSON_ALLOC_H
#define FASTJSON_ALLOC_H

#include "yyjson.h"

/* Use for yyjson allocations to enforce memory_limit and request-end cleanup. */
extern const yyjson_alc fastjson_php_alc;

/* Stack-backed bump allocator for short JSON inputs (<= 1024 B): serves the
 * single yyjson_read_opts chunk from the stack and falls back to Zend MM on
 * overflow. Only valid when the parsed doc is freed before the caller returns. */
#define FASTJSON_SMALL_ALC_THRESHOLD 1024
#define FASTJSON_SMALL_ALC_BUF_SIZE  4096

typedef struct {
    yyjson_alc alc;
    size_t used;
    ZEND_SET_ALIGNED(16, unsigned char bytes[FASTJSON_SMALL_ALC_BUF_SIZE]);
} fastjson_small_alc;

void fastjson_small_alc_init(fastjson_small_alc *s);

#endif /* FASTJSON_ALLOC_H */
