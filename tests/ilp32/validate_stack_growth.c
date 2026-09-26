/*
 +----------------------------------------------------------------------+
 | Bounded model of yyjson's validate-only depth-stack growth guard.    |
 +----------------------------------------------------------------------+
 | tests/validate_stack_growth_guard.phpt (LP64) and                    |
 | tests/validate_stack_growth_ilp32.phpt (ILP32) prepend the real       |
 | `push_ctn` macro lifted out of vendor/yyjson/yyjson.c to this file, so |
 | the harness below drives the shipped macro rather than a copy of it.   |
 |                                                                      |
 | Nothing here allocates: the fake allocator hands back a static arena, |
 | so the boundary cases run in a few kilobytes on both ILP32 (-m32) and  |
 | LP64 builds.                                                         |
 +----------------------------------------------------------------------+
*/

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef size_t usize;
typedef uint64_t u64;
typedef uint8_t u8;

#define USIZE_MAX SIZE_MAX
#define unlikely(x) (x)

typedef struct {
    void *(*malloc)(void *ctx, usize size);
    void *(*realloc)(void *ctx, void *ptr, usize old_size, usize new_size);
    void (*free)(void *ctx, void *ptr);
    void *ctx;
} yyjson_alc;

/* Large enough for every admitted (small-capacity) probe: the largest one
 * writes at index 1024. Rejected probes must not touch it at all. */
#define ARENA_SLOTS 8192
#define ARENA_BYTE 0xA5

static u64 arena[ARENA_SLOTS];
static usize alloc_calls;
static usize alloc_bytes;
static int alc_returns_null;

static void arena_reset(void) {
    memset(arena, ARENA_BYTE, sizeof(arena));
}

static int arena_is_canary(void) {
    const unsigned char *p = (const unsigned char *)arena;
    size_t i;
    for (i = 0; i < sizeof(arena); i++) {
        if (p[i] != ARENA_BYTE) return 0;
    }
    return 1;
}

static void *alc_malloc(void *ctx, usize size) {
    (void)ctx;
    alloc_calls++;
    alloc_bytes = size;
    return alc_returns_null ? NULL : (void *)arena;
}

static void *alc_realloc(void *ctx, void *ptr, usize old_size, usize size) {
    (void)ctx;
    (void)ptr;
    (void)old_size;
    alloc_calls++;
    alloc_bytes = size;
    return alc_returns_null ? NULL : (void *)arena;
}

static void alc_free(void *ctx, void *ptr) {
    (void)ctx;
    (void)ptr;
}

static const yyjson_alc alc = { alc_malloc, alc_realloc, alc_free, NULL };

static int checks;
static int failures;

static void expect(int cond, const char *what) {
    checks++;
    if (!cond) {
        printf("FAIL: %s\n", what);
        failures++;
    }
}

/*
 * Drive the spliced macro once with a forced growth. `use_heap` picks the
 * realloc branch over the inline-stack malloc branch. `*bytes_out` receives
 * the size the macro asked the allocator for (0 when it never asked).
 * Returns 1 when the macro took its fail_alloc path.
 */
static int probe(usize cap, int use_heap, usize *bytes_out) {
    u64 stack_inline[32];
    u64 *stack_buf = use_heap ? (u64 *)arena : stack_inline;
    usize stack_cap = cap;
    usize depth = cap;
    usize ctn_len = 0;
    u8 is_obj = 0;
    int took_fail = 0;

    memset(stack_inline, 0x5A, sizeof(stack_inline));
    *bytes_out = 0;
    alloc_calls = 0;
    alloc_bytes = 0;
    alc_returns_null = 0;
    arena_reset();

    {
        push_ctn(0);
        goto push_ctn_done;
    fail_alloc:
        took_fail = 1;
    push_ctn_done:
        ;
    }
    *bytes_out = alloc_bytes;
    return took_fail;
}

/* A realloc that returns NULL is an error, never a write. */
static int probe_failed_alloc(void) {
    u64 stack_inline[32];
    u64 *stack_buf = (u64 *)arena;
    usize stack_cap = 1024;
    usize depth = 1024;
    usize ctn_len = 0;
    u8 is_obj = 0;
    int took_fail = 0;

    memset(stack_inline, 0x5A, sizeof(stack_inline));
    arena_reset();
    alloc_calls = 0;
    alc_returns_null = 1;
    {
        push_ctn(0);
        goto alloc_done;
    fail_alloc:
        took_fail = 1;
    alloc_done:
        ;
    }
    alc_returns_null = 0;
    return took_fail;
}

int main(void) {
    usize bytes;
    usize cap;
    int took;

    /*
     * 1. The arithmetic the finding rests on, on a 32-bit usize: doubling a
     *    capacity of 2^28 and scaling by sizeof(u64) yields a zero-byte
     *    request. The guard must reject the capacity instead of asking.
     */
    if (sizeof(usize) == 4) {
        usize raw = (usize)(((usize)1 << 29) * sizeof(u64));
        printf("unguarded new_cap*sizeof(u64) at cap=2^28 is %lu\n",
               (unsigned long)raw);
        expect(raw == 0, "unguarded ILP32 byte size must wrap to 0");
        took = probe((usize)1 << 28, 1, &bytes);
        expect(took == 1, "cap 2^28 must be rejected");
        expect(alloc_calls == 0, "no allocation may be attempted at cap 2^28");
        expect(arena_is_canary() == 1, "no write may follow a rejected growth");
    }

    /*
     * 2. First capacity whose byte size overflows, and the largest capacity
     *    whose doubling still fits. Both must be rejected before the
     *    allocator is reached.
     */
    cap = USIZE_MAX / sizeof(u64) + 1;
    took = probe(cap, 1, &bytes);
    expect(took == 1, "cap above USIZE_MAX/sizeof(u64) must be rejected");
    expect(alloc_calls == 0, "no allocation at the byte-size boundary");
    expect(arena_is_canary() == 1, "no write at the byte-size boundary");

    cap = USIZE_MAX / 2;
    took = probe(cap, 1, &bytes);
    expect(took == 1, "cap USIZE_MAX/2 must be rejected on byte size");
    expect(alloc_calls == 0, "no allocation at cap USIZE_MAX/2");
    expect(arena_is_canary() == 1, "no write at cap USIZE_MAX/2");

    /*
     * 3. First capacity whose doubling overflows.
     */
    cap = USIZE_MAX / 2 + 1;
    took = probe(cap, 1, &bytes);
    expect(took == 1, "cap above USIZE_MAX/2 must be rejected");
    expect(alloc_calls == 0, "no allocation at the doubling boundary");
    expect(arena_is_canary() == 1, "no write at the doubling boundary");

    took = probe(USIZE_MAX, 1, &bytes);
    expect(took == 1, "cap USIZE_MAX must be rejected");
    expect(alloc_calls == 0, "no allocation at cap USIZE_MAX");
    expect(arena_is_canary() == 1, "no write at cap USIZE_MAX");

    /*
     * 4. Admitted growth still works on both widths: the realloc branch
     *    (heap-backed stack) and the malloc branch (inline stack).
     */
    took = probe(1024, 1, &bytes);
    expect(took == 0, "cap 1024 must grow");
    expect(alloc_calls == 1, "cap 1024 must reach the allocator once");
    expect(bytes == (usize)(2048 * sizeof(u64)),
           "cap 1024 must request the doubled byte size");
    expect(arena[1024] == 0, "cap 1024 must push at the old depth");
    expect(((const unsigned char *)arena)[1024 * sizeof(u64) - 1] == ARENA_BYTE,
           "cap 1024 must not write past the pushed slot");

    took = probe(32, 0, &bytes);
    expect(took == 0, "the inline stack must grow at cap 32");
    expect(alloc_calls == 1, "the inline branch must call malloc once");
    expect(bytes == (usize)(64 * sizeof(u64)),
           "the inline branch must request the doubled byte size");
    expect(arena[32] == 0, "the inline branch must push at depth 32");

    /*
     * 5. A failed allocation is an error, not a write into a block the
     *    caller no longer owns.
     */
    took = probe_failed_alloc();
    expect(took == 1, "a NULL realloc must take the alloc error path");
    expect(alloc_calls == 1, "the failed realloc must be attempted once");
    expect(arena_is_canary() == 1, "no write may follow a failed realloc");

    printf("usize is %u bits\n", (unsigned)(sizeof(usize) * 8));
    printf("%s: %d checks, %d failures\n",
           failures ? "FAIL" : "PASS", checks, failures);
    return failures ? 1 : 0;
}
