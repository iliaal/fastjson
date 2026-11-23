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

static bool fastjson_small_alc_owns(const fastjson_small_alc *s, const void *ptr)
{
    uintptr_t p = (uintptr_t)ptr;
    uintptr_t lo = (uintptr_t)s->bytes;
    uintptr_t hi = lo + FASTJSON_SMALL_ALC_BUF_SIZE;
    return p >= lo && p < hi;
}

static void *fastjson_small_alc_malloc(void *ctx, size_t size)
{
    fastjson_small_alc *s = (fastjson_small_alc *)ctx;
    /* Reserve at least 16 bytes for size == 0 so a request at used == BUF_SIZE
     * never returns the one-past-end pointer `hi` (outside fastjson_small_alc_owns). */
    size_t req = UNEXPECTED(size == 0) ? 1u : size;
    size_t aligned = (req + 15u) & ~(size_t)15u;
    if (EXPECTED(aligned >= req
            && aligned <= FASTJSON_SMALL_ALC_BUF_SIZE - s->used)) {
        void *ptr = s->bytes + s->used;
        s->used += aligned;
        return ptr;
    }
    return fastjson_alc_malloc(NULL, size);
}

static void *fastjson_small_alc_realloc(void *ctx, void *ptr,
                                        size_t old_size, size_t size)
{
    fastjson_small_alc *s = (fastjson_small_alc *)ctx;
    if (fastjson_small_alc_owns(s, ptr)) {
        void *next = fastjson_alc_malloc(NULL, size);
        if (UNEXPECTED(next == NULL)) {
            return NULL;
        }
        size_t copy = old_size < size ? old_size : size;
        if (copy > 0) {
            memcpy(next, ptr, copy);
        }
        return next;
    }
    return fastjson_alc_realloc(NULL, ptr, old_size, size);
}

static void fastjson_small_alc_free(void *ctx, void *ptr)
{
    fastjson_small_alc *s = (fastjson_small_alc *)ctx;
    if (fastjson_small_alc_owns(s, ptr)) {
        return;
    }
    efree(ptr);
}

void fastjson_small_alc_init(fastjson_small_alc *s)
{
    s->alc.malloc = fastjson_small_alc_malloc;
    s->alc.realloc = fastjson_small_alc_realloc;
    s->alc.free = fastjson_small_alc_free;
    s->alc.ctx = s;
    s->used = 0;
}
