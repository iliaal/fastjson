# fastjson benchmarks

Throughput comparison of fastjson vs ext/json on canonical JSON corpora.

## Running

```sh
# 1. Fetch the data (gitignored, ~10MB).
bench/fetch-data.sh

# 2. Build fastjson (release build, no -O0).
phpize && ./configure --enable-fastjson && make -j$(nproc)

# 3. Run the harness.
php -d extension=$(pwd)/modules/fastjson.so bench/run.php

# Optional: pass a custom data dir or iteration count.
php -d extension=... bench/run.php bench/data 200
```

The output is markdown. Pipe to a file to capture, or just commit
`bench/baseline.md` after a clean run.

## Methodology

- **Per case:** 50 iterations of `(encode | decode | validate)` on each
  file, timed with `hrtime(true)`. Slowest 10% dropped (warmup +
  scheduler noise). Median reported.
- **Throughput denominator:** the source JSON byte size (matches the
  simdjson / yyjson conventions so cross-repo numbers are comparable).
- **Encode is timed independently from decode:** the harness decodes
  each file once up front, then times `encode($value)` only. Decode
  rows time the raw `decode($json)` only. No shared cost.
- **Side-by-side:** every case is measured against `ext/json` in the
  same process. Same allocator pressure, same CPU state, same input.
- **Aggregate row:** sum of per-file medians, NOT a re-run on the
  concatenated corpus. So the throughput is roughly the
  size-weighted average rather than the unweighted mean of the
  per-file numbers.

## Data

Pulled by `bench/fetch-data.sh` from
[`crazyxman/simdjson_php/jsonexamples`](https://github.com/crazyxman/simdjson_php/tree/master/jsonexamples).
That directory mirrors Milo Yip's nativejson-benchmark suite, which is
the same corpus used by simdjson, yyjson, RapidJSON, nlohmann -- so
fastjson's numbers are apples-to-apples with their published numbers.

Subset:

| File | Size | Shape |
|---|---|---|
| apache_builds.json   | 124 KB | small CI status response |
| canada.json          | 2.2 MB | float-heavy GeoJSON (worst case for fp formatting) |
| citm_catalog.json    | 1.7 MB | large catalog, many strings + integer keys |
| github_events.json   | 64 KB  | small typical API response |
| gsoc-2018.json       | 3.3 MB | deep + flat strings, larger doc |
| instruments.json     | 215 KB | nested instruments dataset |
| numbers.json         | 147 KB | number-heavy boundary cases |
| random.json          | 499 KB | mixed types, moderate size |
| stringifiedphp.json  | 140 KB | PHP-derived data round-tripped through ext/json |
| twitter.json         | 617 KB | deeply nested mixed (Twitter API) |
| twitterescaped.json  | 549 KB | same, with `\u` escapes for non-ASCII |
| update-center.json   | 533 KB | Jenkins update center catalog |

## Latest baseline

See [`baseline.md`](./baseline.md). Aggregate numbers across the full
14.8 MB / 15-file large corpus (CPU: i9-13950HX, **release build of
both PHP and fastjson**: `--disable-debug`, `-O2`, `Debug Build => no`).

> ⚠️ **Build matters**: a debug build of either extension inflates the
> apparent fastjson speedup by 5-7x because ext/json's hand-rolled
> scanner gets large gains from `-O2` while yyjson's already-optimized
> code gets less. Always benchmark with `--disable-debug` builds for
> meaningful numbers. If the reported speedup looks like 10x+ on
> decode/encode, your PHP is debug-built.

### Throughput

| Operation | fastjson | ext/json | speedup |
|---|---|---|---|
| Decode (stdClass)     | 533 MB/s   | 209 MB/s | **2.55x** |
| Decode (assoc array)  | 526 MB/s   | 217 MB/s | **2.42x** |
| Encode                | 599 MB/s   | 169 MB/s | **3.54x** |
| Validate              | 1,312 MB/s | 253 MB/s | **5.18x** |

### Memory peak (sum across all 21 files: large + small)

| Operation | fastjson | ext/json | fast/ext |
|---|---|---|---|
| Decode (stdClass)     |  98 MB  | 56 MB    | **1.74x** |
| Decode (assoc array)  |  96 MB  | 55 MB    | **1.75x** |
| Encode                |  63 MB  | 11 MB    | **5.58x** |
| Validate              |  15 MB  | 151 KB   | **101x**  |

The validate row reflects vendor patch P-002 (see
[`vendor/yyjson/PATCHES.md`](../vendor/yyjson/PATCHES.md)) which
adds a no-tree validation mode to yyjson. Pre-patch numbers were
40.85 MB / 277x; the patch eliminates the val_hdr buffer
(~2.7× memory reduction) and incidentally makes validate 2.5×
faster from removing the alloc + realloc-growth path.

**fastjson trades memory for speed.** yyjson's two-stage model means
decode and encode each build a yyjson tree alongside the
zval/zend_string output; the peak heap holds both. Validate is the
extreme case: ext/json's validator is a streaming parser (constant
memory regardless of input size); fastjson calls `yyjson_read_opts`
which builds and immediately frees a full doc, so peak ≈ doc size.

For most callers this is a fine tradeoff -- modern boxes have RAM,
JSON-heavy code is CPU-bound, and the absolute fastjson peak (40 MB
peak validate across the entire 15 MB corpus aggregate) is well
within typical PHP memory budgets. Worth knowing if you're
validate-heavy on giant inputs in tight `memory_limit` settings.

Re-run after non-trivial encoder/decoder changes to catch
regressions; commit the new `baseline.md` alongside the change.

## Notes on what's being measured

- **Decode** chews PHP's allocator: every decoded value is a zval and
  every container is a `zend_array` / `zend_object`. fastjson's gain
  here is yyjson's parser (faster than ext/json's hand-rolled scanner)
  PLUS the bulk hash-load path (`Z_OBJPROP_P` + `zend_hash_update`
  bypasses the per-property `write_property` dispatch). Decode is the
  most allocator-bound op; the speedup ceiling is partly set by Zend's
  arena allocator, not yyjson. Memory: ~1.5x ext/json because yyjson
  builds a doc that lives until the walk completes alongside the
  zval tree.

- **Encode** is dominated by the writer (yyjson has a custom dtoa and
  a SIMD-friendly escape-table approach). The PHP side is just walking
  HashTables. fastjson's much larger encode speedup reflects this:
  yyjson's writer is significantly faster than ext/json's
  smart_str-based one, and the cost dispersion narrows as input gets
  weirder (canada.json, with thousands of floats, hits ext/json
  extremely hard at 7.5 MB/s vs fastjson's 208 MB/s -- 28x). Memory:
  ~5x ext/json because we build a `yyjson_mut_doc` from the zval
  tree before writing it out.

- **Validate** is the cleanest *speed* comparison: just the parser,
  no zval construction. yyjson hits ~1 GB/s on most inputs vs
  ext/json's ~50 MB/s. But it's the *worst* memory comparison:
  ext/json's validator never builds a tree (constant ~80 B regardless
  of input size); yyjson's `yyjson_read_opts` always builds a full
  doc, so fastjson's peak validate memory scales linearly with input
  size. todos/001 considered a custom byte-level validator; deferred
  until a real caller cares.

- **Per-call latency (small corpus)** matters when calling encode /
  decode at high QPS on small payloads. fastjson's overhead floor is
  ~1.5 µs (function dispatch + ZPP + yyjson_doc_new); ext/json's is
  ~2-3 µs. For 50-500 byte payloads, the fixed-cost ratio dominates
  over yyjson's parser-loop wins, so speedups compress to 1.4-2.7x
  in that range.

## Reproducing on a release build

The default `phpize` flow inherits CFLAGS from the PHP it's run
against. If your PHP is `--enable-debug`, your fastjson `.so` ends
up with `-O0 -g`. To check:

```sh
# What the running PHP was built with:
php -i | grep 'Debug Build'        # should say "no"

# What flags fastjson was built with:
grep '^CFLAGS' Makefile             # should include -O2
```

If either says debug or `-O0`, rebuild against a release-mode PHP:

```sh
phpize --clean
/path/to/release-php/bin/phpize
./configure --enable-fastjson \
    --with-php-config=/path/to/release-php/bin/php-config
make -j$(nproc)
# Now CFLAGS = -g -O2 (default for non-debug PHP)
```

## Knowingly-not-yet-measured

- ASAN builds (already covered by CI; not a perf measurement target).
- Thread-safety (ZTS) builds.
- Other allocators (Zend's tracked-alloc vs system malloc).
- Pretty-print encode (current numbers are compact-only).
- JsonSerializable encode hot-path (current encode numbers are plain
  arrays / stdClass).
- Decode-without-result (some callers parse just for side effects;
  yyjson's no-allocator-to-PHP path could close the memory gap).
