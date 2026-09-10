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

#endif /* FASTJSON_ALLOC_H */
