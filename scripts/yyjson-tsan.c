/*
 * Threads decode and escape-encode the same non-ASCII document at once so
 * ThreadSanitizer sees every thread run yyjson's one-time SIMD detection.
 * Built and run by scripts/test-yyjson-tsan.sh.
 */
#define _GNU_SOURCE
#include "yyjson.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define THREADS 8
#define ROUNDS 50

static pthread_barrier_t start;
static char doc[4096];

static void *run(void *arg)
{
    (void)arg;
    pthread_barrier_wait(&start);
    for (int i = 0; i < ROUNDS; i++) {
        yyjson_doc *d = yyjson_read(doc, strlen(doc), 0);
        if (!d) {
            return "read failed";
        }
        size_t len;
        char *out = yyjson_write(d, YYJSON_WRITE_ESCAPE_UNICODE
                                        | YYJSON_WRITE_LOWERCASE_HEX, &len);
        yyjson_doc_free(d);
        if (!out) {
            return "write failed";
        }
        int escaped = memmem(out, len, "\\u65e5", 6) != NULL;
        free(out);
        if (!escaped) {
            return "escape path missing";
        }
    }
    return NULL;
}

int main(void)
{
    size_t p = 0;
    memcpy(doc + p, "{\"k\":\"", 6);
    p += 6;
    for (int i = 0; i < 100; i++) {
        memcpy(doc + p, "\xC3\xBC\xE6\x97\xA5", 5);
        p += 5;
    }
    for (int i = 0; i < 40; i++) {
        memcpy(doc + p, "\xE6\x97\xA5", 3);
        p += 3;
    }
    memcpy(doc + p, "\"}", 3);

    pthread_t t[THREADS];
    pthread_barrier_init(&start, NULL, THREADS);
    for (int i = 0; i < THREADS; i++) {
        if (pthread_create(&t[i], NULL, run, NULL) != 0) {
            fputs("pthread_create failed\n", stderr);
            return 1;
        }
    }
    int failed = 0;
    for (int i = 0; i < THREADS; i++) {
        void *res;
        pthread_join(t[i], &res);
        if (res) {
            fprintf(stderr, "thread %d: %s\n", i, (const char *)res);
            failed = 1;
        }
    }
    pthread_barrier_destroy(&start);
    if (failed) {
        return 1;
    }
    puts("done");
    return 0;
}
