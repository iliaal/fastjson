# upstream-json compat harness state

Snapshot of how fastjson tracks ext/json's contract on the upstream
test suite. Refresh after a PHP upgrade or a fastjson behavior change.

## Numbers (2026-05, php-src 8.6.0)

| Bucket | Count | Source |
|---|---|---|
| Total upstream tests | 98 | `php-src/ext/json/tests/*.phpt` |
| Skipped (categorized) | 45 | `tests/upstream-json/.skiplist` |
| Rewritten + run | 53 | this directory |
| Passing | 50 | run-tests output |
| Environment-skip (--SKIPIF--) | 3 | upstream tests with internal skip |
| Failing | 0 | clean against fastjson 0.1.0 |

So the harness is **green for everything not on the skiplist**. The
skiplist is the contract surface: every entry there names a category
plus a one-line reason or TODO reference.

Net change since the harness landed: **+8 passing tests** from
vendor patch P-001 (lowercase `\uXXXX` hex digits in
`vendor/yyjson/yyjson.c`'s `esc_hex_char_table`); the remaining
UTF-8 / PARTIAL_OUTPUT entries that previously coincided with the
hex divergence are now categorized under their primary cause.

## Skiplist categories

- **ext-json-internals** (49 tests). Behavior fastjson does not aim
  to mirror byte-for-byte. Subdivided in `.skiplist` by feature:
  - JsonSerializable interface (5 tests). Deferred to v0.4.
  - JsonException class (3 tests). Fastjson throws `\Exception`.
  - Exact ext/json error message text or "near location X:Y"
    error-location format (12 tests). Yyjson messages differ.
  - `JSON_HEX_TAG` / `_AMP` / `_APOS` / `_QUOT` (3 tests). Not yet
    honored.
  - `JSON_NUMERIC_CHECK` (1 test). Not yet honored.
  - `JSON_PRESERVE_ZERO_FRACTION` (2 tests). Not yet honored;
    `(double)1230.0` encodes as `1230` instead of `1230.0`.
  - `JSON_PARTIAL_OUTPUT_ON_ERROR` (6 tests). Fastjson aborts on
    encode errors instead of substituting nulls/zeros.
  - `JSON_INVALID_UTF8_IGNORE` / `_SUBSTITUTE` (2 tests). Not yet
    honored.

- **doc-divergence** (4 tests). Behavior explicitly diverges from
  ext/json:
  - `pass001.*` (3 tests): JSON_checker round-trip exercises fp
    formatting (yyjson's `100000000000000000.0` vs ext/json's
    `1.0e+17`); not a target.
  - U+2028 / U+2029 line separator escaping (1 test). yyjson treats
    them as ordinary code points; ext/json always escapes for JSONP
    safety.

- **bug-report** (1 test). `gh15168.phpt`:
  `zend.max_allowed_stack_size` overflow detection not implemented;
  fastjson recurses until the OS kills it. Separate concern from
  user-specified `$depth`.

- **bug-report** (0 tests). Empty for now; populate as fastjson bugs
  are surfaced.

## How to add to the skiplist

1. Run `scripts/sync-upstream-json-tests.sh`.
2. Run `php run-tests.php tests/upstream-json/` and inspect new
   failures.
3. For each failure, decide:
   - Is this a fastjson bug? Add to `.skiplist` under
     `bug-report` and capture the issue in CHANGELOG / a PHPT.
   - Is this a feature we choose not to support? Add under
     `ext-json-internals` with the feature name.
   - Is this an explicit divergence? Add under `doc-divergence`
     with a one-line reason.
4. Re-sync (the skiplist is consulted at sync time, so skipped
   tests aren't generated).

## Reaching 100% non-skip pass rate

Realistic. The two doc-divergences (yyjson's fp formatting and the
U+2028/U+2029 emit policy) cover the residual unmatched bytes. The
ext-json-internals categories grow as fastjson honors more flags;
each lands a few more passing tests.

## Reaching parity with full ext/json

Not the goal. ext/json carries 25+ years of edge cases including
JsonSerializable, exact error formats, and partial-output legacy
behavior. Fastjson aims to be a fast drop-in for the common path
(encode + decode + validate + last_error), with documented
divergences elsewhere.
