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

The output is markdown. Redirect it to a file, or commit it as
`bench/baseline.md` after a clean run.

## Review-performance harness

`bench/run.php` measures the corpus files only. `bench/review-performance.php`
covers the paths tuning work touches most: large-string encode across the threshold boundaries, pointer splice, merge patch, tolerant
decode, and parse-error position reporting. Its committed reference is
`bench/review-baseline.md`.

```sh
php -d extension=$(pwd)/modules/fastjson.so bench/review-performance.php \
    [samples] [target_ms] [/case-name-regex/]
```

It validates each selected case's result before timing it and exits non-zero on
an invalid or unmatched filter, so a case that starts returning `false` cannot
read as a speedup.

Compare builds within one session. Absolute numbers move with machine load;
two runs minutes apart can differ by 40% on the same binary. Build the previous
release and the working tree, then alternate them in one session and take the
best of each:

```sh
git worktree add ../fj-prev <previous-tag>
(cd ../fj-prev && phpize && ./configure --enable-fastjson \
    --with-php-config=$(which php-config) && make -j$(nproc))

for pass in 1 2; do
  for so in "$(pwd)/modules/fastjson.so" ../fj-prev/modules/fastjson.so; do
    taskset -c 4 php -d extension="$so" -d memory_limit=-1 \
        bench/review-performance.php 9 100 > "bench-$(basename $(dirname $(dirname $so)))-$pass.json"
  done
done
```

Interleaving the two builds cancels thermal drift and background load that a
sequential A-then-B run would attribute to the code. If a change tunes a
scan/copy trade-off, check ARM too; its crossover points differ from x86_64.

## Methodology

- 100 iterations of `(encode | decode | validate)` on each file, timed with
  `hrtime(true)`. The slowest 10% are dropped (warmup and scheduler noise)
  and the median is reported.
- Throughput is divided by the source JSON byte size, the same convention
  simdjson and yyjson use, so numbers compare across projects.
- Encode is timed separately from decode: the harness decodes each file
  once up front, then times `encode($value)` only. Decode rows time
  `decode($json)` only.
- Every case runs against `ext/json` in the same process, with the same
  input.
- The aggregate row sums per-file medians instead of re-running on the
  concatenated corpus, so its throughput is roughly the size-weighted
  average of the per-file numbers.

## Data

Pulled by `bench/fetch-data.sh` from
[`crazyxman/simdjson_php/jsonexamples`](https://github.com/crazyxman/simdjson_php/tree/master/jsonexamples).
That directory mirrors Milo Yip's nativejson-benchmark suite, the corpus
simdjson, yyjson, RapidJSON, and nlohmann use, so fastjson's numbers
compare directly with theirs.

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

## Recorded baseline

[`baseline.md`](./baseline.md) records fastjson **0.6.0 with yyjson 0.12.0**
on PHP 8.4.22-dev. The tables below summarize that historical run, not a
measurement of the current checkout. They cover the full 14.8 MB / 15-file
large corpus (CPU: i9-13950HX, **release build of both PHP and fastjson**:
`--disable-debug`, `-O2`, `Debug Build => no`). Re-run the harness to measure
a newer build; do not compare different versions as if they were one run.

> ⚠️ A debug build of either extension inflates the apparent fastjson
> speedup by 5-7x, because ext/json's hand-rolled scanner gains more
> from `-O2` than yyjson does. Benchmark with `--disable-debug` builds.
> If decode or encode shows a 10x+ speedup, your PHP is debug-built.

### Throughput

| Operation | fastjson | ext/json | speedup |
|---|---|---|---|
| Decode (stdClass)     | 428 MB/s | 165 MB/s | **2.59x** |
| Decode (assoc array)  | 410 MB/s | 172 MB/s | **2.39x** |
| Encode                | 748 MB/s | 135 MB/s | **5.56x** |
| Validate              | 994 MB/s | 197 MB/s | **5.04x** |

### Memory peak (sum across all 21 files: large + small)

| Operation | fastjson | ext/json | fast/ext |
|---|---|---|---|
| Decode (stdClass)     | 97.81 MB | 56.37 MB | **1.74x** |
| Decode (assoc array)  | 96.38 MB | 54.94 MB | **1.75x** |
| Encode                | 11.92 MB | 11.24 MB | **1.06x** |
| Validate              | 14.91 MB | 150.6 KB | **101.40x** |

The historical validate row predates the no-copy reader. P-002 removed the
value tree, but at that point yyjson still copied the whole input into a
padded working buffer. That is why this run reports 14.91 MB / 101.40x;
those figures do not describe current `fastjson_validate()` memory use.

Current validation reads the caller's input without copying the whole
buffer (P-008), and P-009 preserves that design while improving scanning.
The documented P-008 measurement reduced `canada.json` validation peak from
about 2.25 MB to 64 bytes on its release build. This is a separate per-file
measurement, not a replacement aggregate for the table above. See
[`vendor/yyjson/PATCHES.md`](../vendor/yyjson/PATCHES.md#p-008-validate-without-copying-the-input)
for the implementation, measurement and flag restrictions.

Validation still uses a small result stub and nesting state; sufficiently
deep inputs grow the nesting stack on the heap, and long numeric tokens at
the input tail can need temporary storage. The no-copy path is not a claim
of zero allocation or constant memory for every possible input.

Decode still holds yyjson's parsed document beside the emerging zval tree,
so it trades memory for speed. Encode writes directly to `smart_str`
without a yyjson value tree. Measure these operations on the target build
rather than treating the historical ratios as current guarantees.

Re-run after non-trivial encoder/decoder changes to catch
regressions; commit the new `baseline.md` alongside the change.

## Notes on what's being measured

- **Decode** is allocator-heavy: every decoded value is a zval and
  every container is a `zend_array` / `zend_object`. fastjson gains
  from yyjson's parser (faster than ext/json's hand-rolled scanner)
  and the bulk hash-load path (`Z_OBJPROP_P` + `zend_hash_update`
  bypasses the per-property `write_property` dispatch). Decode is the
  most allocator-bound op; the speedup ceiling is partly set by Zend's
  arena allocator, not yyjson. Memory: ~1.5x ext/json because yyjson
  builds a doc that lives until the walk completes alongside the
  zval tree.

- **Encode** uses the direct-write encoder (`fastjson_directwrite.c`):
  one-stage zval → `smart_str` via yyjson's `write_number` /
  `write_string_to_buf` primitives (no intermediate `yyjson_mut_doc`).
  Memory is close to ext/json (~1.06× aggregate in `baseline.md`).
  Throughput comes from yyjson's scalar writer and from skipping
  per-value mut-tree allocation.

- **Validate** is the cleanest speed comparison: only the parser runs, with
  no zval construction. P-002 skips the yyjson value tree; the historical
  run above reports ~994 MB/s. Current builds also avoid the whole-input
  copy via P-008 and use P-009's faster scanner. Benchmark the current build
  for its throughput and memory rather than extrapolating from that run.

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
make clean    # important when switching PHP builds; rebuilds vendored yyjson too
make -j$(nproc)
# Now CFLAGS = -g -O2 (default for non-debug PHP)
```

## Approaches tried and rejected

Measured with callgrind instruction counts (Ir) on a release build, because
wall-clock time on the WSL dev host varies by 30% or more between runs.

- **Decode without the input copy (vendor patch P-009, reverted in 0.9.0).**
  String values pointed into the caller's buffer when the document had no
  escapes. A `zend_string` has one NUL after its data, but yyjson's reader
  relies on 4 bytes of padding, so the pretty-print reader and the comment
  skipper read past the end of the buffer. A long input that ended on a page
  boundary crashed PHP. The same reader made number-heavy decode 40-80%
  slower, in return for about 10% less peak memory.
- **Build zvals during the read (`fused-decode` branch, 0bb52b6, deleted).**
  A SAX-style reader (patches P-010 and P-011) built zvals from callbacks,
  never allocated yyjson's value array, and reused slots in large
  containers. Peak memory dropped 48% against 0.9.0 (geometric mean over the
  bench corpus), and `citm_catalog.json` peaked below ext/json. Decode cost
  94% more instructions (+8% to +191% per file). The design builds on P-009,
  so every number and literal needs a bounds-checked reader, and every value
  goes through a function-pointer callback. With aliasing turned off it still
  cost 35% more instructions for 31% less memory. On `canada.json`, the
  bounded number reader alone ran 46.7M instructions, against 54.9M for
  master's whole decode. The only plausible reuse is an opt-in low-memory
  decode flag, built on the P-008 reader without aliasing and with inlined
  readers. Expect about 30% less peak memory than the default decode and
  15-50% more instructions.

## Not yet measured

- ASAN builds (already covered by CI; not a perf measurement target).
- Thread-safety (ZTS) builds.
- Other allocators (Zend's tracked-alloc vs system malloc).
- Pretty-print encode (current numbers are compact-only).
- JsonSerializable encode hot-path (current encode numbers are plain
  arrays / stdClass).
- Decode-without-result (some callers parse just for side effects;
  yyjson's no-allocator-to-PHP path could close the memory gap).
