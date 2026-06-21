# Changelog

All notable changes to fastjson are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- `fastjson_last_error_pos()` and `fastjson_last_error_info()`: surface the byte offset and 1-based line/column of the most recent parse error (yyjson computed it, fastjson previously discarded it).
- `fastjson_pointer_exists()`: report whether an RFC 6901 JSON Pointer resolves, distinguishing present-but-null from absent, without materializing into PHP.
- `fastjson_pointer_set()`: set the value at an RFC 6901 JSON Pointer and return the re-serialized document, editing a yyjson mutable doc to avoid a full decode/re-encode round-trip.

### Build

- Prebuilt `.so` binaries for Linux glibc (x86_64, arm64) and macOS (arm64) on PHP 8.4/8.5, published as release assets; PIE installs these instead of source-building on those targets.

## [0.4.0] - 2026-06-11

### Added

- `fastjson_pointer_get(string $json, string $pointer, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed`: read a single value from a JSON document by [RFC 6901](https://www.rfc-editor.org/rfc/rfc6901) JSON Pointer (`/users/0/email`; the empty pointer `""` selects the whole document). Only the referenced subtree is materialized into PHP. A missing path or malformed pointer returns null with the error state left clear (not a JSON error); a parse error returns null with `fastjson_last_error()` set, or throws under `JSON_THROW_ON_ERROR`. `$associative`, `$depth`, and `$flags` (including `FASTJSON_DECODE_RELAXED`) match `fastjson_decode()`.
- `fastjson_merge_patch(string $target, string $patch, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed`: apply an [RFC 7386](https://www.rfc-editor.org/rfc/rfc7386) JSON Merge Patch and return the merged document as a PHP value. Objects merge recursively, a non-object patch replaces the target wholesale, and a `null` member deletes the corresponding key. Returns a PHP value (not a string) so output flows through the single `fastjson_encode()` path — pass the result to it for byte-consistent JSON. A parse error in either operand returns null with `fastjson_last_error()` set, or throws under `JSON_THROW_ON_ERROR`.
- `FASTJSON_DECODE_RELAXED` decode flag: tolerate the JSONC subset that `ext/json` rejects — line (`//`) and block (`/* */`) comments, trailing commas, and a leading UTF-8 BOM. fastjson-only; there is no `JSON_*` counterpart. Well-formed JSON decodes identically with or without the flag. Pass it in `$flags`, e.g. `fastjson_decode($jsonc, true, 512, FASTJSON_DECODE_RELAXED)`. Backed by yyjson's `ALLOW_COMMENTS | ALLOW_TRAILING_COMMAS | ALLOW_BOM` read flags, so parsing stays robust rather than relying on a pre-pass scrubber.
- PHP 8.1 support (lowered the minimum from 8.3).
- `fastjson_file_decode()` and `fastjson_file_encode()`: read or write a JSON file in one call, collapsing the `fastjson_decode(file_get_contents($f), ...)` and `file_put_contents($f, fastjson_encode(...))` patterns. Signatures mirror the in-memory functions, so `$flags` and `$depth` behave identically:
  - `fastjson_file_decode(string $filename, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed`
  - `fastjson_file_encode(string $filename, mixed $value, int $flags = 0, int $depth = 512): bool`

  Both read and write through the PHP streams layer with the request default stream context, so stream wrappers, `open_basedir`, and `stream_context_set_default()` apply, matching `file_get_contents()`/`file_put_contents()`. I/O failures are silent (no warning): `fastjson_file_decode()` returns null with `fastjson_last_error()` set, `fastjson_file_encode()` returns false. An empty file decodes like `fastjson_decode("")`. `JSON_THROW_ON_ERROR` throws on JSON parse/encode errors but not on filesystem errors. Implements [php-src#22137](https://github.com/php/php-src/issues/22137).

## [0.3.0] - 2026-05-19

### Fixed

- Use-after-free in `fastjson_encode` for PHP 8.4 objects whose property has a SET hook but no GET hook and is not virtual. The engine's trivial-read fast path returns a borrowed pointer to the backing field; the previous stash logic released a refcount it never owned and freed the backing zend_string while the object still pointed at it. ASAN regression added. Surfaced by `/codesage-review` (fnd_f77a591f).
- 32-bit `zend_long` overflow in `dw_emit_double`'s integer-valued-double shortcut. On 32-bit PHP, `fastjson_encode(1e10)` could emit INT32-saturated garbage instead of a valid JSON number; the shortcut bound is now gated on `SIZEOF_ZEND_LONG`. Surfaced by `/codesage-review` (fnd_23254098).
- ext/json parity for integer-valued doubles between `1e15` and `1e17`. The double shortcut bound was `1e15`, so `fastjson_encode(1e16)` fell through to yyjson's REAL writer and emitted `"10000000000000000.0"` while `json_encode(1e16)` emits `"10000000000000000"`. The 64-bit bound now widens to a strict `< 1e17`, matching the cutoff where `php_gcvt` itself switches to scientific notation. `1.5e16`, `2.5e16`, `9.99e16`, and the negative range round-trip byte-identically to `json_encode`.

### Performance

- `JSON_HEX_TAG`/`HEX_AMP`/`HEX_APOS`/`HEX_QUOT` now scan first and skip the rewrite + temp allocation entirely when no candidates exist. When candidates exist, growth is reserved exactly (5 extra bytes per hit) instead of the 6× worst case. Defensive callers asserting HEX flags on payloads that don't contain the substituted characters no longer pay the rewrite cost. Benchmark: ~4× speedup on the no-hit case (532 µs → 125 µs on 1k strings); ~13% regression on the all-hit case from the extra scan pass. (CR-002)
- `fastjson_decode` with `JSON_INVALID_UTF8_IGNORE` / `SUBSTITUTE` now scans each string and object key with a no-allocation UTF-8 validator first, and only invokes the sanitizer on byte sequences that actually need replacement. Valid UTF-8 inputs no longer pay a per-string sanitize allocation and copy. Benchmark: ~36% speedup on object-heavy clean-UTF-8 decode with IGNORE (964 µs → 612 µs). (CR-003)
- `dw_emit_double`'s integer-valued-double shortcut checks the cheap range bound before calling `floor()`. Non-integer or out-of-range doubles in number-heavy arrays no longer pay libm per element. (CR-004)

## [0.2.1] - 2026-05-11

### Build

- Added `config.w32` so the php-windows-builder action can build
  Windows DLLs. 0.2.0 shipped without one and the
  `release-windows.yml` workflow couldn't compile the extension.
  No source changes; identical behavior to 0.2.0 on every platform.

## [0.2.0] - 2026-05-11

### Added

- `JSON_INVALID_UTF8_IGNORE` and `JSON_INVALID_UTF8_SUBSTITUTE` honored
  on both encode and decode, with ext/json-matching semantics: IGNORE
  strips invalid bytes, SUBSTITUTE replaces with U+FFFD. Encode follows
  `php_next_utf8_char`'s UTR-36 maximal-subpart advance rule; decode
  does per-byte advance, mirroring ext/json's parser state machine.
  Both sides also reproduce ext/json's asymmetric precedence when both
  bits are set (encode prefers IGNORE, decode prefers SUBSTITUTE).
  Decode dispatches to a separate sanitizing walker only when a flag
  is set, keeping the no-flag hot path branch-free.

### Fixed

- `JSON_HEX_QUOT` produced unterminated JSON for strings ending in `\`.
  The post-write substitution scan misread the second byte of an
  escaped `\\` followed by the closing `"` as a `\"` escape. The
  scan now walks JSON escape sequences explicitly.
- Virtual properties without a get hook (PHP 8.4) were being read by
  the encoder; `dw_emit_object` now skips them per ext/json.
- `fastjson_validate` now classifies a non-ASCII parse-error byte as
  `JSON_ERROR_UTF8` only when the bytes are malformed UTF-8. Valid
  UTF-8 sequences that aren't valid JSON (e.g. bare `é` at top level)
  stay `JSON_ERROR_SYNTAX`, matching ext/json. Decode shared the same
  fix.
- `fastjson_validate("", -1)` now short-circuits to `false` (matching
  ext/json) instead of raising `ValueError` before the empty-input
  check.
- `-0.0` now encodes as `"-0"` (or `"-0.0"` with `PRESERVE_ZERO_FRACTION`)
  instead of `"0"`. The integer-valued-double shortcut had been
  zero-extending the sign.
- `last_error` is cleared before an argument `ValueError` raises on
  bad `$depth`, in both `fastjson_decode` and `fastjson_validate`
  (matches ext/json's `json_decode` / `json_validate` contract).

### Build

- Vendored yyjson symbols no longer leak into the .so dynamic table.
  `-fvisibility=hidden` was being overridden by yyjson's
  `__attribute__((visibility("default")))` per-symbol; the build now
  also passes `-Dyyjson_api=` so the macro's `#ifndef` guard skips
  the visibility-default branch. After this change `nm -D
  modules/fastjson.so | grep -v get_module` returns empty.

### Tests

- ~14 previously-skipped ext/json upstream tests now pass under the
  fastjson harness, mostly from the `JSON_INVALID_UTF8_*` work
  (`json_encode_invalid_utf8`, `json_decode_invalid_utf8`, `bug43941`,
  `bug55543`, `bug61978`, `bug63737`, `bug68567`, `bug72787`,
  `bug73991`). New tests cover the fixes: `encode_hex_flags.phpt`,
  `encode_virtual_no_get.phpt`, `error_cleared_on_value_error.phpt`,
  `invalid_utf8_flags.phpt`. Extended: `validate_utf8.phpt`,
  `validate_depth.phpt`, `encode_preserve_zero_fraction.phpt`.

## [0.1.0] - 2026-05-10

Initial release. Fast JSON encode, decode, and validate for PHP 8.3+,
backed by yyjson 0.12.0.

### Added

- `fastjson_encode(mixed $value, int $flags = 0, int $depth = 512): string|false`.
  Direct-write one-stage encoder: walks zvals straight into a `smart_str`
  buffer using yyjson's primitives for numbers (`yyjson_write_number`) and
  strings (`yyjson_write_string_to_buf`). Flags honored: `JSON_PRETTY_PRINT`,
  `JSON_UNESCAPED_SLASHES`, `JSON_UNESCAPED_UNICODE`, `JSON_FORCE_OBJECT`,
  `JSON_PARTIAL_OUTPUT_ON_ERROR`, `JSON_HEX_TAG`, `JSON_HEX_AMP`,
  `JSON_HEX_APOS`, `JSON_HEX_QUOT`, `JSON_NUMERIC_CHECK`,
  `JSON_PRESERVE_ZERO_FRACTION`, `JSON_INVALID_UTF8_IGNORE`,
  `JSON_INVALID_UTF8_SUBSTITUTE`, `JSON_THROW_ON_ERROR`.

- `fastjson_decode(string $json, ?bool $associative = null, int $depth = 512, int $flags = 0): mixed`.
  yyjson_doc → zval walker covering null, bool, int, uint (widens to double
  above `PHP_INT_MAX`), float, string, array, object (`stdClass` or
  associative array). Object property tables written via `Z_OBJPROP_P` to
  bypass per-property handler dispatch. Flags honored:
  `JSON_BIGINT_AS_STRING`, `JSON_OBJECT_AS_ARRAY`, `JSON_INVALID_UTF8_IGNORE`,
  `JSON_INVALID_UTF8_SUBSTITUTE`, `JSON_THROW_ON_ERROR`.

- `fastjson_validate(string $json, int $depth = 512, int $flags = 0): bool`.
  Streaming validate via vendor patch P-002 (`YYJSON_READ_VALIDATE_ONLY`):
  no doc tree built, peak memory drops 2.7× vs yyjson's default read path.

- `fastjson_last_error(): int` and `fastjson_last_error_msg(): string`.
  Error codes match `JSON_ERROR_*` int values byte-for-byte; messages are
  yyjson's diagnostics (more descriptive than ext/json's static strings).

- `FASTJSON_ERROR_NONE`, `_DEPTH`, `_STATE_MISMATCH`, `_CTRL_CHAR`, `_SYNTAX`,
  `_UTF8`, `_RECURSION`, `_INF_OR_NAN`, `_UNSUPPORTED_TYPE`,
  `_INVALID_PROPERTY_NAME`, `_UTF16` constants. Values intentionally match
  `JSON_ERROR_*` so callers can use either set.

- `JsonSerializable` and `\JsonException` integration. `jsonSerialize()`
  return values feed back through the encoder; `JSON_THROW_ON_ERROR` raises
  `\JsonException` with `fastjson_last_error()` unchanged (per ext/json's
  contract).

- Depth and stack-overflow guards on encode and decode. `$depth <= 0` or
  `> INT_MAX` raises `ValueError` on decode/validate; encode follows
  ext/json's lazy-fail contract. Stack walks gated on
  `zend_call_stack_overflowed(EG(stack_limit))` so deeply chained inputs
  fail cleanly instead of being killed by the OS.

- Bundled yyjson 0.12.0 (MIT) with three local patches in
  `vendor/yyjson/PATCHES.md`:
  - **P-001**: lowercase `\uXXXX` escape table for byte-equality with
    ext/json. Submitted upstream as ibireme/yyjson#263, #264.
  - **P-002**: `YYJSON_READ_VALIDATE_ONLY` flag and `read_root_validate`
    parser entry point. No tree built; aux stack for container tracking.
  - **P-003**: public `yyjson_write_string_to_buf()` wrapper around the
    internal escape-table writer. Required for the one-stage encoder.
    Submitted upstream as ibireme/yyjson#265, #266.

- yyjson allocator (`fastjson_php_alc`) routes every malloc/realloc/free
  through Zend's `emalloc`/`erealloc`/`efree`. JSON allocations participate
  in `memory_limit` accounting and request-scoped cleanup.

- Benchmark harness at `bench/` measuring encode + decode + validate vs
  ext/json on the simdjson_php canonical corpus (15 large + 6 small files,
  14.81 MB total). On i9-13950HX with release builds of both PHP and
  fastjson: decode 2.66×, encode 6.06×, validate 5.10× vs ext/json on
  large-corpus aggregate. Per-file numbers and reproduction recipe at
  `bench/README.md` and `bench/baseline.md`.

- Compat harness at `tests/upstream-json/`: 53 rewritten phpt tests from
  `php-src/ext/json/tests/*.phpt` run alongside fastjson's native suite.
  `tests/upstream-json/.skiplist` categorizes every upstream test fastjson
  does not target byte-for-byte. Re-sync via
  `scripts/sync-upstream-json-tests.sh` after PHP upgrades.

### Tradeoffs

- Decode and validate hold the yyjson doc in memory alongside results.
  Decode peak ≈ 1.7× ext/json. Validate peak ≈ 101× ext/json's streaming
  validator (constant ~80 bytes), but already 2.7× better than yyjson's
  stock read path thanks to P-002.
- Encode is one-stage; peak ≈ 1.06× ext/json.

### Known divergences from ext/json

- Large/scientific doubles emit yyjson's `100000000000000000.0` where
  ext/json emits `1.0e+17`. Not targeted; documented in
  `tests/upstream-json/.skiplist`.
- U+2028 / U+2029 line separators emitted as ordinary code points
  (yyjson default). ext/json always escapes for JSONP safety.

[Unreleased]: https://github.com/iliaal/fastjson/compare/0.4.0...HEAD
[0.4.0]: https://github.com/iliaal/fastjson/releases/tag/0.4.0
[0.3.0]: https://github.com/iliaal/fastjson/releases/tag/0.3.0
[0.2.1]: https://github.com/iliaal/fastjson/releases/tag/0.2.1
[0.2.0]: https://github.com/iliaal/fastjson/releases/tag/0.2.0
[0.1.0]: https://github.com/iliaal/fastjson/releases/tag/0.1.0
