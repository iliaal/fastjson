# upstream-json compat harness state

Snapshot of how fastjson tracks ext/json's contract on the upstream
test suite. Refresh after a PHP upgrade or a fastjson behavior change.
The `.skiplist` is the authoritative contract surface; these numbers and
category notes summarize it.

## Numbers (php-src 8.6.0)

| Bucket | Count | Source |
|---|---|---|
| Total upstream tests | 98 | `php-src/ext/json/tests/*.phpt` |
| Skipped (categorized) | 36 | `tests/upstream-json/.skiplist` |
| Rewritten + run | 62 | this directory |
| Passing | 62 | run-tests output |
| Environment-skip (--SKIPIF--) | 0 | none currently |
| Failing | 0 | clean against current fastjson |

So the harness is **green for everything not on the skiplist**: all 62
rewritten tests pass. The skiplist is the contract surface; every entry
names a category plus a one-line reason or TODO reference.

Since v0.1.0 the rewritten set has grown from 53 to 62 as fastjson
honored more flags. Vendor patch P-001 (lowercase `\uXXXX` hex digits)
landed the first batch; honoring `JSON_HEX_*`, `JSON_NUMERIC_CHECK`,
`JSON_PARTIAL_OUTPUT_ON_ERROR`, and `JSON_INVALID_UTF8_IGNORE/SUBSTITUTE`
moved the rest off the skiplist. Remaining skiplist entries fail for
reasons orthogonal to the flag itself (error-message text, error-code
numbering, NUL-byte handling) or are explicit divergences.

## Skiplist categories

- **ext-json-internals** (31 tests). Behavior fastjson does not aim to
  mirror byte-for-byte. By feature:
  - JsonSerializable + var_export / print_r / serialize interplay
    (5 tests: `json_encode_recursion_03..06`, `serialize`). The core
    `jsonSerialize()` path is honored; these exercise ext/json-internal
    dumping behavior fastjson does not reproduce.
  - Error-message text ext/json owns verbatim, including the
    `Syntax error near location X:Y` and `Malformed UTF-8 characters`
    wording (14 tests: the 10 `error_location_*`, `bug54058`,
    `json_validate_002`, `json_validate_004`, `007`). yyjson's
    diagnostics differ; codes still match.
  - JsonException message-text divergence (2 tests:
    `json_decode_exceptions`, `json_encode_exceptions`). fastjson throws
    `\JsonException` and preserves global error state on throw; only the
    message wording differs.
  - UTF-8 error code numbering and NUL-byte handling (6 tests:
    `bug54484`, `bug61537`, `bug62010`, `bug68546`, `bug69187`,
    `bug66021`). `JSON_ERROR_CTRL_CHAR` (code 3) is not surfaced by
    yyjson, and ext/json strips NUL bytes from stdClass property names.
  - `JSON_PRESERVE_ZERO_FRACTION` knock-on (2 tests:
    `json_encode_basic`, `json_encode_pretty_print2`). `(double)1230.0`
    encodes as `1230`, not `1230.0`.
  - `JSON_PARTIAL_OUTPUT_ON_ERROR` with `jsonSerialize()` returning
    `$this` (2 tests: `json_encode_recursion_01`, `_02`). The flag is
    honored generally; this needs ext/json's special case where the
    returned zval IS the original and it falls through to property
    serialization.

- **doc-divergence** (4 tests). Behavior fastjson explicitly diverges
  from:
  - `pass001.*` (3 tests): JSON_checker round-trip whose residual diff
    is floating-point formatting (yyjson `100000000000000000.0` vs
    ext/json `1.0e+17`); not a target.
  - U+2028 / U+2029 line separator escaping (1 test). yyjson treats them
    as ordinary code points; ext/json always escapes for JSONP safety.

- **bug-report** (1 test). `bug77843`: memory leak in encode +
  `JsonSerializable` + `PARTIAL_OUTPUT`; a fastjson-side bug to
  investigate, not a contract divergence.

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

Already there: all 62 rewritten tests pass. Moving more upstream tests
off the skiplist depends on honoring the remaining flag knock-ons
(`PRESERVE_ZERO_FRACTION`, the `jsonSerialize() returns $this` case) and
deciding whether to match ext/json's exact error-message text, which is
the single largest skiplist category.

## Reaching parity with full ext/json

Not the goal. ext/json carries 25+ years of edge cases including exact
error-message formats, error-code numbering, and partial-output legacy
behavior. fastjson aims to be a fast drop-in for the common path
(encode + decode + validate + last_error), with documented divergences
elsewhere.
