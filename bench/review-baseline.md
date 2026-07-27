# Review-performance baseline

Per-case medians from `bench/review-performance.php`, the harness that covers
the paths `bench/run.php` does not: large-string encode, pointer splice, merge
patch, tolerant decode, and parse-error position reporting.

- PHP 8.4.22-dev (NTS release build)
- CPU: 13th Gen Intel(R) Core(TM) i9-13950HX
- 9 samples per case, 100 ms target, best of 2 interleaved passes, `taskset` pinned
- Reference: fastjson 0.6.0 built from the same tree against the same PHP

Absolute nanoseconds are machine-specific and are not a pass/fail gate. The
column that matters is the delta against the previous release, measured in the
same session (see bench/README.md).

| case | 0.6.0 (ns) | current (ns) | delta |
|---|---:|---:|---:|
| `merge_patch_2k` | 6,233,647 | 337,987 | -94.6% |
| `pointer_set_string_1m` | 609,247 | 217,005 | -64.4% |
| `encode_ascii_512k` | 280,068 | 99,968 | -64.3% |
| `encode_late_quote_512k` | 273,852 | 99,377 | -63.7% |
| `decode_tolerant_clean_1m` | 967,856 | 361,768 | -62.6% |
| `encode_ascii_below_1m` | 558,165 | 216,079 | -61.3% |
| `encode_ascii_1m` | 549,590 | 212,866 | -61.3% |
| `pointer_set_root_array_10k` | 209,585 | 103,340 | -50.7% |
| `decode_error_tail_512k` | 377,460 | 190,975 | -49.4% |
| `encode_true` | 46 | 27 | -41.1% |
| `encode_null` | 46 | 28 | -39.8% |
| `encode_ascii_256k` | 75,237 | 49,116 | -34.7% |
| `encode_int` | 51 | 35 | -32.3% |
| `pointer_set_array_10k` | 267,380 | 183,292 | -31.4% |
| `encode_late_quote_256k` | 72,736 | 49,945 | -31.3% |
| `decode_tolerant_invalid_tail_1m` | 1,970,176 | 1,448,933 | -26.5% |
| `decode_tolerant_strings_8b` | 893,367 | 707,888 | -20.8% |
| `decode_tolerant_strings_1b` | 2,168,228 | 1,776,136 | -18.1% |
| `decode_packed_scalar_10k` | 95,069 | 79,719 | -16.1% |
| `encode_double` | 61 | 54 | -11.9% |
| `file_decode_16m` | 27,287,686 | 25,190,867 | -7.7% |
| `decode_packed_mixed_2k` | 136,167 | 126,138 | -7.4% |
| `encode_unicode_512k` | 820,693 | 761,955 | -7.2% |
| `encode_hex_32k` | 78,524 | 73,056 | -7.0% |
| `decode_object_5k` | 137,926 | 128,730 | -6.7% |
| `encode_unicode_256k` | 212,665 | 200,437 | -5.7% |
| `encode_unicode_1m` | 1,693,539 | 1,630,009 | -3.8% |
| `encode_unicode_128k` | 104,468 | 101,636 | -2.7% |
| `encode_ascii_64k` | 18,699 | 18,221 | -2.6% |
| `encode_escaped_1m` | 1,814,503 | 1,783,199 | -1.7% |
| `encode_ascii_below_256k` | 75,708 | 74,715 | -1.3% |
| `encode_ascii_128k` | 36,912 | 36,569 | -0.9% |
| `validate_strings_1b` | 246,172 | 244,288 | -0.8% |
| `decode_assoc_object_5k` | 119,916 | 120,093 | +0.1% |
| `file_get_plus_decode_16m` | 25,356,187 | 25,452,534 | +0.4% |
| `pointer_set_hex_32k` | 76,051 | 76,514 | +0.6% |
| `encode_escaped_128k` | 136,376 | 137,356 | +0.7% |
| `encode_escaped_256k` | 421,947 | 425,491 | +0.8% |
| `encode_escaped_512k` | 855,056 | 865,190 | +1.2% |
| `validate_strings_8b` | 222,129 | 227,828 | +2.6% |
| `pointer_set_leaf` | 109,891 | 114,477 | +4.2% |
