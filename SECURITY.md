# Security policy

fastjson is a namespaced drop-in alternative to ext/json, backed by a
vendored, statically compiled yyjson. It encodes, decodes, validates,
pointer-queries, and merge-patches JSON. The realistic threat surface
is untrusted JSON, or untrusted RFC 6901 pointers, RFC 7386 patches,
and file paths, reaching the native C decode/encode/validate paths,
which run in-process under the same trust model as ext/json.

## Supported versions

| Version | Supported          |
|---------|--------------------|
| 0.4.x   | :white_check_mark: |

Pre-1.0: the current minor gets security fixes, and the API may still
move between minors until 1.0.

## Reporting a vulnerability

**Do not file a public GitHub issue for security vulnerabilities.**

Use GitHub's private security advisory feature at
<https://github.com/iliaal/fastjson/security/advisories/new>
or email Ilia Alshanetsky <ilia@ilia.ws> directly.

Please include:

- Affected fastjson version (`php -r 'echo phpversion("fastjson");'`)
- PHP version (`php -v`)
- A minimal reproducing case (PHP code plus the JSON, pointer, or patch
  that triggers it, small enough to inline in the report)
- Impact: crash / RCE / info disclosure / DoS / etc.
- Whether you've coordinated disclosure with anyone else

Acknowledgement within 7 days, fix or status update within 30. Once a
fix is released the advisory becomes public.

## Scope

In scope:

- Crashes, memory corruption, or read-after-free reachable from PHP
  through any decode/encode/validate entry point: `fastjson_decode()`,
  `fastjson_encode()`, `fastjson_validate()`, `fastjson_pointer_get()`,
  `fastjson_pointer_exists()`, `fastjson_pointer_set()`,
  `fastjson_merge_patch()`, and the `fastjson_file_*` variants.
- Buffer or integer overflows in fastjson's own C (`fastjson_decode.c`,
  `fastjson_encode.c`, `fastjson_directwrite.c`, `fastjson_alloc.c`).
- Bugs in the vendored yyjson (`vendor/yyjson/yyjson.c`) reachable
  through fastjson's public API. fastjson ships this code statically,
  so report it here; we coordinate upstream where appropriate.
- Depth and recursion handling. The `$depth` argument is a
  denial-of-service boundary; stack exhaustion or overflow reachable
  below the configured limit is in scope.
- `open_basedir` or stream-wrapper bypasses in `fastjson_file_encode()`
  / `fastjson_file_decode()`, which go through PHP's stream layer.
- Arginfo / ZPP mismatches that cause undefined behavior reachable from
  PHP.

Out of scope:

- Parser strictness disagreements. The `FASTJSON_DECODE_RELAXED` flag
  intentionally accepts comments, trailing commas, and a leading BOM;
  that widened grammar is by design, not a vulnerability.
- Resource exhaustion from decoding intentionally huge documents within
  `memory_limit`. Allocations route through Zend MM and obey
  `memory_limit`; overflow paths that bypass those limits are in scope.
- Behavioral differences from ext/json that don't cross a memory-safety
  or DoS boundary.
