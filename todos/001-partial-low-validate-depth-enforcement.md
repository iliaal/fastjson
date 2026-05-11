# 001: fastjson_validate ignores `$depth` on the success path

**Status:** Partially resolved. Encode and decode walkers enforce
`$depth` byte-for-byte with ext/json (see commit `8fa1d0b` in the
pre-squash history, rolled into v0.1.0). Argument validation
(`$depth <= 0` / `$depth > INT_MAX` raises `ValueError`) is also in
place across all three entry points. **The validate success path
is the only remaining gap** -- yyjson's internal recursion guard is
much higher than 512, so `fastjson_validate(deeply_nested, 512)`
returns `true` where ext/json returns `false` with `JSON_ERROR_DEPTH`.

**Severity:** Low
**Source:** scaffold-time decision (commit f1b6de6)
**File:** `fastjson.c` (`fastjson_validate`)
**Confidence:** 1.00 (intentional, not a bug)
**Effort:** Small to Medium depending on chosen path
**Tags:** compat, ext-json-parity

## Why validate is deferred

ext/json's `json_validate` runs a custom streaming parser that tracks
container depth inline; the depth check is free. fastjson's validate
uses yyjson's `YYJSON_READ_VALIDATE_ONLY` mode (vendor patch P-002),
which doesn't expose per-value depth to the caller. To enforce
`$depth` on success we'd have to walk the result doc after parsing,
which roughly doubles the success-path cost. The bench shows fastjson
validate at 1.3 GB/s vs ext/json's 245 MB/s; halving that to
~650 MB/s for the rare caller who actually depends on the depth cap
is a poor trade.

## Options

1. **Live with yyjson's internal recursion guard, document the gap.**
   (Currently in place.) yyjson rejects pathological nesting well
   before stack exhaustion. The cap mismatch only matters when a
   caller relies on the exact `$depth` value as a security bound
   against untrusted input.

2. **Opt-in depth walk via flag.** Add a non-ext/json flag like
   `FASTJSON_VALIDATE_DEPTH_STRICT` that triggers the walk only when
   set. Default-off keeps the hot path fast; security-conscious
   callers opt in.

3. **Wait for upstream.** yyjson 0.13+ may add a
   `YYJSON_READ_MAX_DEPTH` flag. The `.upstream/yyjson.yml` freshness
   gate already pings releases. If it lands, switch to it and skip
   option 2.

## Recommendation

Option 1 until a concrete caller asks for depth enforcement, or
option 3 if yyjson ships the flag in 0.13. Option 2 is the fallback.

## Already resolved on encode and decode

Both `fastjson_encode` and `fastjson_decode` count container depth
inside the walker and abort with `FASTJSON_ERROR_DEPTH` (mapped to
`JSON_ERROR_DEPTH`) on overflow. `tests/decode_depth.phpt` and the
encode equivalents cover the contract. The validate-only gap above
is the last piece.
