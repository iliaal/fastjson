# 002: fastjson_encode emits \uXXXX with uppercase hex; ext/json uses lowercase

**Status:** Resolved 2026-05-10 via vendor patch P-001 (see
`vendor/yyjson/PATCHES.md`). The bundled `esc_hex_char_table` was
modified in-place to use `'a'..'f'` instead of `'A'..'F'`. fastjson
now byte-matches ext/json on `\uXXXX` escapes.

Re-apply the patch when upgrading yyjson; reproduction sed is
documented in `vendor/yyjson/PATCHES.md`.

The historical analysis below is preserved for context.

---

**Severity:** Low
**Source:** scaffold-time observation (commit landing fastjson_encode)
**File:** `fastjson_encode.c` (write path) + upstream yyjson 0.12
**Confidence:** 1.00 (verified by inspection)
**Effort:** Small (post-process) or upstream (yyjson flag)
**Tags:** compat, ext-json-parity, encode

## Problem

When emitting non-ASCII characters as escape sequences (the
fastjson default, matching ext/json's default), yyjson writes
uppercase hex digits:

```
fastjson_encode("é")  =>  "é"
json_encode("é")      =>  "é"
```

JSON spec allows either case (RFC 8259 §7), so this is semantically
equivalent. But callers comparing encoded output byte-for-byte
against ext/json -- e.g. cache-key fingerprints, content-hash
deduplication, snapshot tests, golden-file tests -- will see a
divergence.

The compat harness (task #16) will surface this as test failures
across many ext/json upstream tests.

## Options

1. **Post-process the yyjson output buffer.** Walk the bytes,
   detect `\uXXXX` sequences, lowercase the hex digits before
   constructing the zend_string. Linear pass, minor cost on the
   already-O(n) encode path. Predictable, no upstream dependency.

2. **Patch yyjson with a case option.** Forward a tiny patch to
   ibireme/yyjson adding `YYJSON_WRITE_UNICODE_ESCAPE_LOWERCASE`.
   Cleaner long-term; the patch is ~5 lines (the hex digit lookup
   table). Carries the cost of a vendor patch we'd have to re-apply
   on every yyjson upgrade until upstream merges it.

3. **Document and live with it.** Add a note in the stub and the
   README; tell the compat harness to round-trip through decode
   when comparing against ext/json output.

## Recommendation

Option 3 for v0.3 (ship encode now, document the divergence).
Option 1 if the compat harness or a real caller forces parity.
Option 2 only if upstream is receptive -- propose it once we have
data on how often callers care.

## Where to look when deciding

- yyjson's escape table: `vendor/yyjson/yyjson.c`, search for the
  `enc_table` / `esc_hex` lookup.
- ext/json's lowercase choice: `php-src/ext/json/json_encoder.c`,
  in `php_json_escape_string`.
