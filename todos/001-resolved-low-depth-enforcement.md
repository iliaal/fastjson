# 001: fastjson_validate ignores the $depth parameter

**Severity:** Low
**Source:** scaffold-time decision (commit f1b6de6)
**File:** `fastjson.c` (`fastjson_validate`)
**Confidence:** 1.00 (intentional, not a bug)
**Effort:** Small to Medium depending on chosen path
**Tags:** compat, ext-json-parity

## Problem

`fastjson_validate(string $json, int $depth = 512, int $flags = 0)` accepts
`$depth` for source-compatibility with ext/json's `json_validate`, but the
parameter is currently `(void)`-cast and ignored. yyjson 0.12 has no
parse-time depth flag analogous to ext/json's depth cap.

ext/json's contract: `json_validate("[" . str_repeat("[", 600) . ...,
$depth=512)` returns `false` with `JSON_ERROR_DEPTH`.
fastjson_validate currently parses successfully (yyjson's internal recursion
guard is much higher than 512) and returns `true`.

This is a divergence from ext/json's behaviour. Anything caller code that
relies on the depth cap as a safety bound (e.g. for untrusted input) gets
weaker enforcement than it asks for.

## Options

1. **Live with yyjson's internal recursion guard, document the gap.**
   Cheapest. Update the stub doc to surface the divergence; do nothing in C.
   Fine for callers who don't depend on the exact cap; bad for those who
   do.

2. **Post-parse depth walk.** After `yyjson_read_opts` returns, walk the
   resulting `yyjson_doc` counting nesting; if it exceeds `$depth`, free
   and return `false` with `JSON_ERROR_DEPTH`. Doubles the validate cost
   on the success path (yyjson already walked once); on the failure path
   it's free (we never reach the walk). Probably the right answer; benchmark
   the slowdown on a deeply-nested but small input before committing.

3. **Wait for upstream.** yyjson 0.13+ may add a `YYJSON_READ_MAX_DEPTH`
   flag. Check the changelog at each upstream bump (the
   `.upstream/yyjson.yml` freshness gate already pings releases). If it
   lands, switch to it and skip option 2.

## Recommendation

Option 1 until a real caller asks for depth enforcement, or option 3 if
yyjson ships the flag in 0.13. Option 2 is the fallback if neither
materializes.

## Same gap will exist for fastjson_decode

When `fastjson_decode` lands, it has the identical `$depth` parameter and
the identical gap. Resolve both together; don't ship a half-fix that only
covers one entry point.
