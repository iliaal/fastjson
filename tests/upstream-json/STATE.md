# upstream-json compat harness state

Snapshot of how fastjson tracks ext/json's contract on the upstream
test suite. Refresh after a PHP upgrade or a fastjson behavior change.
The `.skiplist` is the authoritative contract surface; these numbers and
category notes summarize it.

## Numbers (php-src 8.6.0)

| Bucket | Count | Source |
|---|---|---|
| Total upstream tests | 100 | `php-src/ext/json/tests/*.phpt` |
| Skipped (categorized) | 36 | `tests/upstream-json/.skiplist` |
| Rewritten + run | 64 | this directory |
| Passing | 64 | run-tests output |
| Environment-skip (--SKIPIF--) | 0 | none currently |
| Failing | 0 | clean against current fastjson |

So the harness is **green for everything not on the skiplist**: all 64
rewritten tests pass. The skiplist is the contract surface; every entry
names a category plus a one-line reason or TODO reference.

Since v0.1.0 the rewritten set has grown from 53 to 64 as fastjson
honored more flags. Vendor patch P-001 (lowercase `\uXXXX` hex digits)
landed the first batch; honoring `JSON_HEX_*`, `JSON_NUMERIC_CHECK`,
`JSON_PARTIAL_OUTPUT_ON_ERROR`, and `JSON_INVALID_UTF8_IGNORE/SUBSTITUTE`
moved the next batch; the `jsonSerialize()`-returns-`$this` special case
moved `json_encode_recursion_01`. Remaining skiplist entries fail for reasons orthogonal to
the flag itself (error-message text, error-code numbering, NUL-byte
handling) or are explicit divergences.

## Skiplist categories

- **ext-json-internals** (32 tests). Behavior fastjson does not aim to
  mirror byte-for-byte. By feature:
  - JsonSerializable + var_export / print_r / serialize interplay
    (5 tests: `json_encode_recursion_03..06`, `serialize`). The core
    `jsonSerialize()` path is honored; these exercise ext/json-internal
    dumping behavior fastjson does not reproduce.
  - `var_dump($this)` *during* `jsonSerialize()` (2 tests:
    `json_encode_recursion_02`, `bug77843`). Fastjson uses ext/json's
    JSON-specific guard on PHP 8.3+, but PHP 8.1/8.2 expose only the generic
    recursion flag. The resulting diagnostic is version-dependent, so one
    generated expectation cannot cover every supported runtime. The
    returns-`$this` behavior and `bug77843`'s use-after-free are covered by
    native `encode_jsonserializable*.phpt` tests.
  - Error-message text ext/json owns verbatim, including the
    `Syntax error near location X:Y` and `Malformed UTF-8 characters`
    wording (15 tests: the 10 `error_location_*`, `bug54058`,
    `json_validate_002`, `json_validate_004`, `007`, `gh22420`). yyjson's
    diagnostics differ; codes still match.
  - JsonException message-text divergence (2 tests:
    `json_decode_exceptions`, `json_encode_exceptions`). fastjson throws
    `\JsonException` and preserves global error state on throw; only the
    message wording differs.
  - UTF-8 error code numbering and NUL-byte handling (6 tests:
    `bug54484`, `bug61537`, `bug62010`, `bug68546`, `bug69187`,
    `bug66021`). yyjson lumps invalid-string failures under one code, so
    both a raw control char (ext/json `JSON_ERROR_CTRL_CHAR`, code 3) and
    an unpaired surrogate (ext/json `JSON_ERROR_UTF16`, code 10) surface
    as `JSON_ERROR_UTF8` (code 5). fastjson still *rejects* both inputs,
    including under `JSON_INVALID_UTF8_IGNORE/SUBSTITUTE` (which relax
    UTF-8 validity, not control-char/structural strictness); only the
    reported code differs. ext/json also strips NUL bytes from stdClass
    property names, which fastjson does not.
  - `JSON_PRESERVE_ZERO_FRACTION` knock-on (2 tests:
    `json_encode_basic`, `json_encode_pretty_print2`). `(double)1230.0`
    encodes as `1230`, not `1230.0`.
  - `serialize_precision` is not honored: fastjson always emits yyjson's
    shortest round-trippable double (matching ext/json's default
    `serialize_precision = -1`). Under a fixed precision (e.g. `4`),
    ext/json rounds (`1.235`) while fastjson emits the full value
    (`1.2345678901234567`). Replacing yyjson's number writer with a
    precision-aware one is out of scope.

- **doc-divergence** (4 tests). Behavior fastjson explicitly diverges
  from:
  - `pass001.*` (3 tests): JSON_checker round-trip whose residual diff
    is floating-point formatting (yyjson `100000000000000000.0` vs
    ext/json `1.0e+17`); not a target.
  - U+2028 / U+2029 line separator escaping (1 test). yyjson treats them
    as ordinary code points; ext/json always escapes for JSONP safety.

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

Already there: all 63 rewritten tests pass. Moving more upstream tests
off the skiplist depends on honoring the remaining flag knock-ons
(`PRESERVE_ZERO_FRACTION`, `serialize_precision`) and deciding whether to
match ext/json's exact error-message text and error-code numbering, which
is the single largest skiplist category.

## Reaching parity with full ext/json

Not the goal. ext/json carries 25+ years of edge cases including exact
error-message formats, error-code numbering, and partial-output legacy
behavior. fastjson aims to be a fast drop-in for the common path
(encode + decode + validate + last_error), with documented divergences
elsewhere.
