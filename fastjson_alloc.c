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

/* yyjson's realloc signature passes both old_size and new size; emalloc's
 * erealloc only needs the new size, so old_size is ignored. yyjson never
 * passes ptr=NULL or size=0 to realloc -- it routes those to malloc/free
 * itself -- so we don't need to special-case them here. */
static void *fastjson_alc_malloc(void *ctx, size_t size)
{
    (void)ctx;
    return emalloc(size);
}

static void *fastjson_alc_realloc(void *ctx, void *ptr, size_t old_size, size_t size)
{
    (void)ctx;
    (void)old_size;
    return erealloc(ptr, size);
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
