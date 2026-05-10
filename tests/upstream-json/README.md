# upstream-json compat harness

These tests are mechanically rewritten copies of
`php-src/ext/json/tests/*.phpt` with `json_*` symbol references
remapped to `fastjson_*`. The harness measures how closely fastjson
tracks ext/json's contract on real-world inputs.

**Do not edit files in this directory by hand.** Regenerate with:

```sh
scripts/sync-upstream-json-tests.sh [$HOME/php-src]
```

| Source PHP version | 8.6.0 |
| Synced count       | 98 |
| Skipped            | 45 (per .skiplist) |

Tests intentionally not rewritten are listed in `.skiplist` with a
one-line reason; categories are documented at the head of that file.
