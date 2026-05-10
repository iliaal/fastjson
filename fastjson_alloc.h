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

/* yyjson allocator that routes every malloc/realloc/free through Zend's
 * per-request heap (emalloc/erealloc/efree). Pass &fastjson_php_alc as
 * the `alc` argument to yyjson_read_opts, yyjson_write_opts, and
 * yyjson_mut_doc_new so JSON allocations participate in PHP's memory
 * limit accounting and are reclaimed at request end. */
extern const yyjson_alc fastjson_php_alc;

#endif /* FASTJSON_ALLOC_H */
