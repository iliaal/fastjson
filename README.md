# fastjson

Fast JSON encode, decode, and validate for PHP 8.3+, backed by
[yyjson](https://github.com/ibireme/yyjson). Drop-in alternative to the
native `ext/json` with a namespaced `fastjson_*` API and
`json_last_error`-compatible error reporting.

> **Status:** pre-release. yyjson 0.12.0 is vendored and linked. The
> full `fastjson_encode` / `fastjson_decode` / `fastjson_validate`
> trio plus `fastjson_last_error` / `_msg` are available. The compat
> harness against `php-src/ext/json/tests/*.phpt` passes everything
> targeting features fastjson aims to mirror; the rest is categorized
> in `tests/upstream-json/.skiplist`.

## Performance

Throughput vs `ext/json` on the full 14.8 MB / 15-file canonical
corpus from simdjson_php's
[jsonexamples](https://github.com/crazyxman/simdjson_php/tree/master/jsonexamples)
(i9-13950HX, **release build of both PHP and fastjson** -- `-O2`,
`Debug Build => no`):

| Operation | fastjson | ext/json | speedup |
|---|---|---|---|
| Decode (stdClass)     | 602 MB/s | 227 MB/s | **2.66x** |
| Decode (assoc array)  | 628 MB/s | 237 MB/s | **2.65x** |
| Encode                | 1,092 MB/s | 180 MB/s | **6.06x** |
| Validate              | 1,352 MB/s | 265 MB/s | **5.10x** |

fastjson trades memory for speed on decode (yyjson's two-stage parser
holds the doc alongside the zval tree). Decode peak is ~1.7x ext/json's
heap. **Encode runs one-stage** (direct-write into smart_str using
yyjson primitives) so encode memory is at near-parity with ext/json
(~1.1x). Validate peaks at ~101x: lower than ext/json's true streaming
validator
(constant ~80 bytes) but already 2.7x better than yyjson's stock
read path thanks to vendor patch P-002. See
[`vendor/yyjson/PATCHES.md`](vendor/yyjson/PATCHES.md). For most
callers the speedup is worth the memory headroom; if you're
validate-heavy on giant inputs in tight `memory_limit` settings,
it's a real consideration.

Methodology, per-file numbers, small-corpus + per-call latency
breakdown, and how to reproduce: see
[`bench/README.md`](bench/README.md) and
[`bench/baseline.md`](bench/baseline.md). A visual side-by-side
including ext/json + PR-120 (SIMD encode) and simdjson_php on the
same PHP 8.6.0-dev build lives at
[`docs/baseline.html`](docs/baseline.html) (clone + open).

## Why another JSON extension

`ext/json` is competent but leaves performance on the table on both the
encode and decode paths. yyjson is one of the fastest portable JSON
libraries available (decode ~1.5-2 GB/s, encode ~1 GB/s on commodity
hardware) and provides a mutable DOM that maps cleanly onto walking a
PHP `HashTable` during encode. fastjson exposes that engine to PHP
without touching `ext/json`, so both can coexist and adoption is opt-in
per call site.

## Install

Once the v0.1 release is published:

```sh
pie install iliaal/fastjson
```

Or build from source:

```sh
phpize
./configure --enable-fastjson
make
sudo make install
echo "extension=fastjson.so" | sudo tee /etc/php/8.3/cli/conf.d/30-fastjson.ini
```

## Usage

```php
$json = fastjson_encode(['hello' => 'world']);     // string|false
$data = fastjson_decode($json, assoc: true);        // mixed
$ok   = fastjson_validate($json);                   // bool

if ($data === null && fastjson_last_error() !== 0) {
    fwrite(STDERR, fastjson_last_error_msg());
}
```

Function signatures track `ext/json` so call sites migrate by
search-and-replace from `json_*` to `fastjson_*`. Honored flags on
encode: `JSON_PRETTY_PRINT`, `JSON_UNESCAPED_SLASHES`,
`JSON_UNESCAPED_UNICODE`, `JSON_FORCE_OBJECT`, `JSON_HEX_TAG`,
`JSON_HEX_AMP`, `JSON_HEX_APOS`, `JSON_HEX_QUOT`,
`JSON_NUMERIC_CHECK`, `JSON_PRESERVE_ZERO_FRACTION`,
`JSON_PARTIAL_OUTPUT_ON_ERROR`, `JSON_THROW_ON_ERROR`. On decode:
`JSON_OBJECT_AS_ARRAY`, `JSON_BIGINT_AS_STRING`, `JSON_THROW_ON_ERROR`.
On validate: `JSON_INVALID_UTF8_IGNORE` (other bits raise `ValueError`
per ext/json's contract). PHP 8.4 property hooks and `JsonSerializable`
are honored. See [`CHANGELOG.md`](CHANGELOG.md) and the divergences
list at the bottom of that file for behavior fastjson does not aim to
mirror byte-for-byte.

## Roadmap

- [x] Vendored yyjson 0.12.0 (MIT) + three local patches in
      [`vendor/yyjson/PATCHES.md`](vendor/yyjson/PATCHES.md)
- [x] `fastjson_encode` / `fastjson_decode` / `fastjson_validate` /
      `fastjson_last_error` / `fastjson_last_error_msg` with
      ext/json-compatible flags and error codes
- [x] Benchmark harness on simdjson_php's canonical corpus, including
      vs `simdjson_php` and ext/json with PR-120 SIMD encode applied
      (see [`bench/README.md`](bench/README.md),
      [`bench/baseline.md`](bench/baseline.md))
- [x] Compat harness against `php-src/ext/json/tests/*.phpt` with
      categorized skiplist (`tests/upstream-json/.skiplist`,
      `tests/upstream-json/STATE.md`)
- [ ] `fastjson_validate` success-path depth enforcement (currently
      argument-validated but the cap is not walked; see
      [`todos/001`](todos/001-partial-low-validate-depth-enforcement.md))
- [ ] Streaming / incremental decode and encode

## License

fastjson is BSD 3-Clause. The bundled yyjson sources are MIT. See
[LICENSE](LICENSE) for the full text of both.
