# Patches applied to vendored yyjson sources

The bundled yyjson sources at `vendor/yyjson/yyjson.{c,h}` are
upstream's tag 0.12.0 with five local modifications documented below.
The upstream `vendor/yyjson/LICENSE` and `vendor/yyjson/CHANGELOG.md`
are unchanged.

The patch files under `vendor/yyjson/patches/` are the canonical replayable
series. `scripts/verify-yyjson-patches.sh` downloads the pinned pristine
release, checks its archive digest, applies the series in order, and compares
the result byte-for-byte with the vendored sources. Run it after any patch or
vendor update. The prose below records rationale and upgrade caveats.

## P-001: lowercase hex digits in `\uXXXX` escape table

**File:** `vendor/yyjson/yyjson.c`
**Region:** `static const u8 esc_hex_char_table[512] = { ... };`
(starts around line 8442 in 0.12.0)

**Reason.** Upstream emits uppercase hex (`é`); ext/json emits
lowercase (`é`). Both are spec-equivalent (RFC 8259 §7), but
fastjson's parity goal with `ext/json` makes byte-equality a target.
yyjson exposes no flag for case selection; the table is hardcoded.

**Patch.** In-place sed over the table region, replacing single-quoted
uppercase letters with their lowercase equivalents.

Reproducible from a clean upstream import:

```sh
START=$(grep -n 'esc_hex_char_table\[512\]' vendor/yyjson/yyjson.c | head -1 | cut -d: -f1)
END=$(awk -v s=$START 'NR>=s && /^};/ { print NR; exit }' vendor/yyjson/yyjson.c)
sed -i "${START},${END}{s/'A'/'a'/g; s/'B'/'b'/g; s/'C'/'c'/g; s/'D'/'d'/g; s/'E'/'e'/g; s/'F'/'f'/g}" vendor/yyjson/yyjson.c
```

The range-bounded sed only touches the table; surrounding code that
uses single-quoted uppercase letters for other purposes is unaffected.

**Verification.**

```sh
make && php -d extension=$(pwd)/modules/fastjson.so -r '
    var_dump(json_encode("héllo") === fastjson_encode("héllo"));
'
# expect: bool(true)
```

The upstream-json compat harness adds ~25 newly-passing tests after
this patch lands.

## P-002: `YYJSON_READ_VALIDATE_ONLY`, no-tree validate mode

**File:** `vendor/yyjson/yyjson.c` (new function `read_root_validate`)
+ `vendor/yyjson/yyjson.h` (new flag `YYJSON_READ_VALIDATE_ONLY`)

**Reason.** yyjson's `yyjson_read_opts` always builds a full
`yyjson_doc` tree, even when the caller only wants to know "is this
JSON well-formed?". `fastjson_validate` was paying this cost: peak
memory ≈ 3× input size for a value it immediately discards. ext/json's
`json_validate` is a streaming parser using ~80 bytes of state.

P-002 adds a forked parser entry point `read_root_validate` that
mirrors `read_root_minify`'s state machine but uses an auxiliary stack
(starts at 32 inline frames × 8 bytes = 256 bytes; grows on demand
via the standard alc) for container-type tracking, instead of yyjson's
slot-based parent-offset trick. The val_hdr buffer is eliminated. All
scalar reads (`read_str`, `read_num`, `read_true`, `read_false`,
`read_null`, `read_inf_or_nan`) share a single stack-allocated
`yyjson_val dummy`. They write to it, but we discard the result.

On success, the parser returns a stub `yyjson_doc` (~64 bytes total)
whose `alc` and `str_pool` are seeded so `yyjson_doc_free()` cleans up
correctly. Caller MUST NOT walk the returned doc; it carries no
values.

**Effect.**
- Validate memory peak (corpus aggregate, 14.81 MB total input):
  40.85 MB → 14.91 MB (2.7× reduction). Per-file: canada.json
  (2.15 MB input) drops from 7.88 MB to 2.15 MB peak.
- Validate throughput (corpus aggregate): 525 MB/s → 1,312 MB/s
  (2.5× faster). Driven by removal of the val_hdr alloc + realloc
  growth path and reduced write traffic.
- vs ext/json memory: ratio drops from 277.78× to 101.40×. The
  remaining ratio is the input-buffer copy yyjson always makes when
  `YYJSON_READ_INSITU` is not set. That's the next ceiling.

**How to apply.** The patch is two-sided (header + impl) and structural
enough that a sed reproducer is impractical. Re-applying after a
yyjson upgrade requires:

1. Add `YYJSON_READ_VALIDATE_ONLY = 1 << 14` to
   `vendor/yyjson/yyjson.h` (after the `YYJSON_READ_JSON5` block).
2. Insert the `read_root_validate` function in `vendor/yyjson/yyjson.c`
   above the existing `// MARK: - JSON Reader (Public)` divider.
3. Add the dispatch in `yyjson_read_opts`:
   ```c
   if (has_flg(VALIDATE_ONLY)) {
       doc = read_root_validate(hdr, cur, eof, alc, flg, err);
   } else if (likely(char_is_ctn(*cur))) { ... existing ... }
   ```

The function's structure tracks `read_root_minify` 1:1: labels
(arr_val_begin, arr_val_end, arr_end, obj_*, doc_end, fail_*) match
upstream's, so a side-by-side diff of the two functions is the easiest
re-port.

**Caveats.**
- Caller must pass `YYJSON_READ_VALIDATE_ONLY` AND must not walk the
  returned doc. Ungated use of the returned `yyjson_doc *` segfaults.
- `YYJSON_READ_NUMBER_AS_RAW` and `YYJSON_READ_BIGNUM_AS_RAW` are
  honored at the parse level (validation accepts them) but their
  side-effects (writing raw text into the doc) are no-ops.
- Top-level scalars defer to `read_root_single` (the existing yyjson
  function) which allocates a tiny ~32-byte doc; not a meaningful
  cost.

## P-003: expose `yyjson_write_string_to_buf()` public API

**Files:** `vendor/yyjson/yyjson.h`, `vendor/yyjson/yyjson.c`

**Reason.** fastjson's direct-write encoder walks PHP zvals straight
into a `smart_str` buffer instead of building a `yyjson_mut_doc` first
(one-stage vs two-stage). To do that without losing yyjson's tight
escape-table writer, we need to call `write_str` on raw byte buffers
from outside yyjson. `yyjson_write_number` is already public via the
upstream API, but no equivalent exists for strings.

**Patch.** Adds a thin public wrapper that delegates to the existing
internal `static_inline write_str()`. yyjson's writer logic is
untouched; the patch only changes its surface.

In `yyjson.h`, after `yyjson_mut_write_number`:

```c
yyjson_api char *yyjson_write_string_to_buf(char *cur,
                                            const char *str,
                                            size_t str_len,
                                            yyjson_write_flag flg);
```

In `yyjson.c`, immediately before the `YYJSON_DISABLE_WRITER` end-
of-block `#endif`:

```c
char *yyjson_write_string_to_buf(char *cur, const char *str, size_t str_len,
                                 yyjson_write_flag flg) {
    bool esc = has_flg(ESCAPE_UNICODE);
    bool inv = has_flg(ALLOW_INVALID_UNICODE);
    const char_enc_type *enc_table = get_enc_table_with_flag(flg);
    return (char *)write_str((u8 *)cur, esc, inv,
                             (const u8 *)str, (usize)str_len, enc_table);
}
```

Caller's destination buffer must have capacity `>= str_len * 6 + 2`
to fit worst-case `\uXXXX` expansion plus surrounding quotes.

**Re-apply recipe on yyjson upgrade.**

1. Apply the header declaration verbatim after the existing
   `yyjson_mut_write_number` inline wrapper.
2. Apply the implementation verbatim immediately before the
   `#undef has_flg` lines that close the writer block in yyjson.c
   (search for `#endif /* YYJSON_DISABLE_WRITER */`).
3. Confirm `get_enc_table_with_flag()` and `write_str()` still have
   the signatures shown above. Upstream has been API-stable on these
   internals across recent releases but verify on each upgrade.

**Caveats.**
- The wrapper does NOT take an allocator argument. It writes into
  caller-provided memory; if the caller wants growth, that's their
  smart_str / yyjson_alc concern.
- No `YYJSON_WRITE_INF_AND_NAN_AS_NULL` handling (string-only path).
- No pretty-print logic (strings have no indentation context).

## P-004: reject control characters in strings under `ALLOW_INVALID_UNICODE`

**File:** `vendor/yyjson/yyjson.c` (the string reader `read_string`)
**Region:** the `else` branch that handles a non-escape, non-quote byte,
carrying the `"unexpected control character in string"` message (around
line 4937 in 0.12.0).

**Reason.** ext/json rejects raw control characters (byte `< 0x20`)
inside JSON strings as `JSON_ERROR_CTRL_CHAR`, *regardless* of its
`JSON_INVALID_UTF8_IGNORE` / `SUBSTITUTE` flags — those relax UTF-8
validity only. yyjson conflates the two: `YYJSON_READ_ALLOW_INVALID_UNICODE`
(which fastjson enables for the INVALID_UTF8 flags) also makes yyjson
*accept* raw control chars in strings. The result was `fastjson_decode()`
and `fastjson_validate()` silently accepting `"[\"a\x01b\"]"` under
`JSON_INVALID_UTF8_IGNORE`.

Fixing this in the reader (rather than post-parse in fastjson) is the
correct layer: the check fires exactly where a string byte is consumed,
so it covers `fastjson_decode`, `fastjson_file_decode`, `fastjson_validate`,
`fastjson_pointer_*`, and `fastjson_merge_patch` uniformly, and it is
comment-aware for free under `RELAXED` (yyjson's comment parser never
routes comment bytes through the string reader, so a quote or control
char inside a `//`/`/* */` comment is not misread as string content).

**Invalid UTF-8 tolerance is preserved.** High bytes (`>= 0x80`) are
handled on a *separate* path (the non-ASCII branch, message
`"invalid UTF-8 encoding in string"`), which still honors
`ALLOW_INVALID_UNICODE`. Only the control-char branch is made
unconditional.

**Patch.** Replace the flag-gated control-char branch:

```c
    } else {
        if (!has_allow(INVALID_UNICODE)) {
            return_err(src, "unexpected control character in string");
        }
        if (src >= eof) return_err(src, "unclosed string");
        *dst++ = *src++;
    }
```

with an unconditional rejection (unclosed strings are already caught
earlier as `"unexpected end of data"`, so only genuine control chars
reach here):

```c
    } else {
        if (src >= eof) return_err(src, "unclosed string");
        return_err(src, "unexpected control character in string");
    }
```

**Verification.**

```sh
make && php -d extension=$(pwd)/modules/fastjson.so -r '
    var_dump(fastjson_decode("[\"a\x01b\"]", true, 512, JSON_INVALID_UTF8_IGNORE));   // NULL
    var_dump(fastjson_validate("[\"a\x01b\"]", 512, JSON_INVALID_UTF8_IGNORE));       // false
    var_dump(fastjson_decode("[\"a\xffb\"]", true, 512, JSON_INVALID_UTF8_IGNORE));   // ["ab"] (utf-8 still tolerated)
    var_dump(fastjson_decode("// x \" y\n[1]", true, 512, FASTJSON_DECODE_RELAXED));  // [1] (comment quote not misread)
'
```

**Re-apply recipe on yyjson upgrade.** Locate the single
`"unexpected control character in string"` return in `read_string` and
drop the `has_allow(INVALID_UNICODE)` gate as shown. Confirm the
high-byte path still carries its own separate
`"invalid UTF-8 encoding in string"` return (that one keeps the flag
gate).

## P-005: tag strings accepted with invalid UTF-8

**Files:** `vendor/yyjson/yyjson.h`, `vendor/yyjson/yyjson.c`
**Region:** `read_str_opt()` and the reserved value-tag definitions

**Reason.** With `YYJSON_READ_ALLOW_INVALID_UNICODE`, yyjson still identifies
each invalid byte while parsing but discards that information. fastjson then
had to validate every decoded string again to discover which strings needed
`JSON_INVALID_UTF8_IGNORE` or `JSON_INVALID_UTF8_SUBSTITUTE`. Clean tolerant
decodes paid for a redundant scan; malformed strings were scanned once to
detect the fault and again to sanitize it.

**Patch.** Reserve tag bit `1 << 5` as
`YYJSON_RESERVED_INVALID_UNICODE`. `read_str_opt()` records either invalid-
Unicode acceptance branch in an internal high bit of its existing local flag
word, then includes the reserved bit in the final string tag. Reusing that word
avoids the register pressure and strict-read regression caused by a dedicated
per-string boolean. The bit is metadata only: the existing type, subtype,
length, and string bytes are unchanged. yyjson's immutable-to-mutable copy
preserves the full tag, so merge-patch output retains the marker too.

fastjson's sanitizing walkers check this bit. Clean strings go directly to the
Zend value; tagged strings go directly through the sanitizer without a
separate well-formedness scan.

**Re-apply recipe on yyjson upgrade.**

1. Add `YYJSON_RESERVED_INVALID_UNICODE` to an unused reserved tag bit.
2. Reserve an internal high bit in `read_str_opt()`'s by-value `flg` word; do
   not add another local variable to this register-sensitive loop.
3. Set the internal bit in both branches that accept invalid UTF-8 under
   `ALLOW_INVALID_UNICODE` (the no-escape and copy/escape paths).
4. Write each normal string tag first, then OR the reserved tag bit inside an
   `unlikely(flg & READ_INVALID_UNICODE_SEEN)` branch.
5. Confirm immutable/mutable value-copy functions still copy the entire tag.

**Verification.** Run the UTF-8 decode, pointer, merge-patch, and upstream
compatibility PHPTs. Benchmark strict validation with short strings and tolerant
decoding with clean strings plus an invalid byte near the end. Strict parsing
must match the pre-patch path, and tolerant decoding must not retain a redundant
full-string scan.

## Build-flag dependencies (not vendor patches)

These aren't edits to vendored sources, but they depend on a specific
invariant in the upstream code. Re-check on every yyjson upgrade.

### `-Dyyjson_api=` neutralizes `visibility("default")`

**File:** `vendor/yyjson/yyjson.h` (read-only invariant)
**Build site:** `config.m4` FASTJSON_CFLAGS

**Reason.** `-fvisibility=hidden` alone doesn't keep yyjson symbols
out of the .so's dynamic table because upstream defines `yyjson_api`
as `__attribute__((visibility("default")))` per-symbol, overriding
the compiler default. Without the workaround, `nm -D` lists ~49
`yyjson_*` / `unsafe_yyjson_*` symbols.

**Mechanism.** `config.m4` passes `-Dyyjson_api=` so the preprocessor
sees the macro as already defined before `yyjson.h` reaches its
`#ifndef yyjson_api` guard (currently at line 322 in 0.12.0). The
visibility-default branch is skipped; symbols inherit
`-fvisibility=hidden`. fastjson is the only caller of yyjson within
the .so, so hidden visibility doesn't affect functionality.

**Re-check on upgrade.**

1. Confirm `yyjson.h` still uses `#ifndef yyjson_api` to guard its
   visibility-default definition. If upstream switches to an
   unconditional `#define`, `-Dyyjson_api=` will trigger a
   redefinition warning and the build flag will need to change (e.g.
   patch the header instead).
2. After building, verify with:
   ```sh
   nm -D --defined-only modules/fastjson.so | grep -v get_module
   ```
   Expected output: empty. Anything else means the override leaked
   again.
