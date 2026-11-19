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
#include "fastjson_alloc.h"

/* yyjson grows its value pool by doubling, so a single infallible request
 * can jump from under memory_limit to past the hard limit, where the engine
 * ends the process. Where the engine queues memory-limit errors, a fallible
 * request that crosses memory_limit queues the error and returns NULL
 * instead, and yyjson fails the read or build. */
static void *fastjson_alc_malloc(void *ctx, size_t size)
{
    (void)ctx;
#ifdef ZEND_HAS_OOM
    return femalloc(size, true);
#else
    return emalloc(size);
#endif
}

static void *fastjson_alc_realloc(void *ctx, void *ptr, size_t old_size, size_t size)
{
    /* yyjson routes NULL pointers and zero sizes to malloc/free itself. */
    (void)ctx;
    (void)old_size;
#ifdef ZEND_HAS_OOM
    return ferealloc(ptr, size, true);
#else
    return erealloc(ptr, size);
#endif
}

static void fastjson_alc_free(void *ctx, void *ptr)
{
    (void)ctx;
    efree(ptr);
}

const yyjson_alc fastjson_php_alc = {
    fastjson_alc_malloc,
    fastjson_alc_realloc,
    fastjson_alc_free,
    NULL
};
