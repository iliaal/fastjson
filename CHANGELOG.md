# Changelog

All notable changes to fastjson are documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/) and this project
adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

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

[Unreleased]: https://github.com/iliaal/fastjson/compare/0.1.0...HEAD
[0.1.0]: https://github.com/iliaal/fastjson/releases/tag/0.1.0
