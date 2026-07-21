# fastjson benchmark

- PHP 8.4.22-dev
- fastjson 0.6.0 (yyjson 0.12.0)
- ext/json 8.4.22-dev
- 100 iterations per case (slowest 10% dropped)
- CPU: 13th Gen Intel(R) Core(TM) i9-13950HX

## Throughput, large corpus

### Decode (objects)

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |     684.4 MB/s |     293.0 MB/s |   2.34x |
| canada.json            | 2.15 MB |     329.7 MB/s |      80.3 MB/s |   4.11x |
| citm_catalog.json      | 1.65 MB |     719.7 MB/s |     331.3 MB/s |   2.17x |
| github_events.json     | 63.6 KB |     789.6 MB/s |     270.4 MB/s |   2.92x |
| gsoc-2018.json         | 3.17 MB |     774.1 MB/s |     255.9 MB/s |   3.02x |
| instruments.json       | 215.2 KB |     719.3 MB/s |     290.2 MB/s |   2.48x |
| marine_ik.json         | 2.85 MB |     225.3 MB/s |     136.2 MB/s |   1.65x |
| mesh.json              | 706.6 KB |     379.3 MB/s |     133.5 MB/s |   2.84x |
| mesh.pretty.json       | 1.50 MB |     558.2 MB/s |     195.9 MB/s |   2.85x |
| numbers.json           | 146.6 KB |     842.2 MB/s |     187.1 MB/s |   4.50x |
| random.json            | 498.5 KB |     378.3 MB/s |     201.4 MB/s |   1.88x |
| stringifiedphp.json    | 139.9 KB |   2,148.9 MB/s |     269.7 MB/s |   7.97x |
| twitter.json           | 616.7 KB |     781.8 MB/s |     280.3 MB/s |   2.79x |
| twitterescaped.json    | 549.2 KB |     694.0 MB/s |     247.6 MB/s |   2.80x |
| update-center.json     | 520.7 KB |     506.9 MB/s |     201.6 MB/s |   2.52x |

### Decode (assoc arrays)

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |     887.1 MB/s |     336.6 MB/s |   2.64x |
| canada.json            | 2.15 MB |     308.6 MB/s |      77.6 MB/s |   3.98x |
| citm_catalog.json      | 1.65 MB |     846.8 MB/s |     381.9 MB/s |   2.22x |
| github_events.json     | 63.6 KB |   1,071.0 MB/s |     305.6 MB/s |   3.50x |
| gsoc-2018.json         | 3.17 MB |     729.2 MB/s |     257.5 MB/s |   2.83x |
| instruments.json       | 215.2 KB |     917.4 MB/s |     316.4 MB/s |   2.90x |
| marine_ik.json         | 2.85 MB |     195.9 MB/s |     152.0 MB/s |   1.29x |
| mesh.json              | 706.6 KB |     405.2 MB/s |     152.4 MB/s |   2.66x |
| mesh.pretty.json       | 1.50 MB |     617.0 MB/s |     192.4 MB/s |   3.21x |
| numbers.json           | 146.6 KB |     785.2 MB/s |     178.5 MB/s |   4.40x |
| random.json            | 498.5 KB |     447.6 MB/s |     223.0 MB/s |   2.01x |
| stringifiedphp.json    | 139.9 KB |   2,341.8 MB/s |     290.2 MB/s |   8.07x |
| twitter.json           | 616.7 KB |     898.2 MB/s |     356.5 MB/s |   2.52x |
| twitterescaped.json    | 549.2 KB |     752.1 MB/s |     235.6 MB/s |   3.19x |
| update-center.json     | 520.7 KB |     476.0 MB/s |     218.1 MB/s |   2.18x |

### Encode

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,219.9 MB/s |     937.4 MB/s |   1.30x |
| canada.json            | 2.15 MB |     546.5 MB/s |      48.3 MB/s |  11.32x |
| citm_catalog.json      | 1.65 MB |   2,260.5 MB/s |   1,944.7 MB/s |   1.16x |
| github_events.json     | 63.6 KB |   1,636.1 MB/s |   1,037.9 MB/s |   1.58x |
| gsoc-2018.json         | 3.17 MB |     956.3 MB/s |     556.9 MB/s |   1.72x |
| instruments.json       | 215.2 KB |   1,509.2 MB/s |   1,459.2 MB/s |   1.03x |
| marine_ik.json         | 2.85 MB |     453.9 MB/s |      95.4 MB/s |   4.76x |
| mesh.json              | 706.6 KB |     587.7 MB/s |      69.5 MB/s |   8.46x |
| mesh.pretty.json       | 1.50 MB |   1,371.7 MB/s |     132.0 MB/s |  10.39x |
| numbers.json           | 146.6 KB |     463.6 MB/s |      38.4 MB/s |  12.08x |
| random.json            | 498.5 KB |     593.2 MB/s |     468.3 MB/s |   1.27x |
| stringifiedphp.json    | 139.9 KB |   2,260.7 MB/s |     569.0 MB/s |   3.97x |
| twitter.json           | 616.7 KB |   1,178.4 MB/s |     770.5 MB/s |   1.53x |
| twitterescaped.json    | 549.2 KB |     913.9 MB/s |     639.3 MB/s |   1.43x |
| update-center.json     | 520.7 KB |     701.2 MB/s |     546.9 MB/s |   1.28x |

### Validate

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,723.4 MB/s |     301.5 MB/s |   5.72x |
| canada.json            | 2.15 MB |     631.2 MB/s |      82.5 MB/s |   7.65x |
| citm_catalog.json      | 1.65 MB |   1,768.1 MB/s |     355.5 MB/s |   4.97x |
| github_events.json     | 63.6 KB |   1,850.2 MB/s |     295.7 MB/s |   6.26x |
| gsoc-2018.json         | 3.17 MB |     850.1 MB/s |     275.2 MB/s |   3.09x |
| instruments.json       | 215.2 KB |   1,981.6 MB/s |     408.5 MB/s |   4.85x |
| marine_ik.json         | 2.85 MB |     808.3 MB/s |     210.8 MB/s |   3.83x |
| mesh.json              | 706.6 KB |   1,039.8 MB/s |     189.9 MB/s |   5.47x |
| mesh.pretty.json       | 1.50 MB |   1,405.0 MB/s |     211.8 MB/s |   6.63x |
| numbers.json           | 146.6 KB |   1,035.3 MB/s |     194.3 MB/s |   5.33x |
| random.json            | 498.5 KB |   1,354.6 MB/s |     302.3 MB/s |   4.48x |
| stringifiedphp.json    | 139.9 KB |   2,340.1 MB/s |     293.8 MB/s |   7.97x |
| twitter.json           | 616.7 KB |   2,283.1 MB/s |     477.4 MB/s |   4.78x |
| twitterescaped.json    | 549.2 KB |   2,047.2 MB/s |     329.7 MB/s |   6.21x |
| update-center.json     | 520.7 KB |   1,763.4 MB/s |     277.4 MB/s |   6.36x |

## Throughput, small corpus

### Decode (objects)

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     256.0 MB/s |     146.4 MB/s |    298 ns |    521 ns |   1.75x |
| demo.json              |  387 B |     618.2 MB/s |     270.4 MB/s |    597 ns |   1.4 µs |   2.29x |
| flatadversarial.json   |   64 B |     212.7 MB/s |     113.0 MB/s |    287 ns |    540 ns |   1.88x |
| repeat.json            | 11.1 KB |     782.0 MB/s |     364.8 MB/s |  13.8 µs |  29.7 µs |   2.14x |
| truenull.json          | 11.7 KB |     862.9 MB/s |     170.7 MB/s |  13.3 µs |  67.0 µs |   5.05x |
| twitter_timeline.json  | 41.2 KB |     743.0 MB/s |     264.0 MB/s |  54.2 µs | 152.6 µs |   2.81x |

### Decode (assoc arrays)

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     290.1 MB/s |     151.4 MB/s |    263 ns |    504 ns |   1.92x |
| demo.json              |  387 B |     674.7 MB/s |     290.2 MB/s |    547 ns |   1.3 µs |   2.33x |
| flatadversarial.json   |   64 B |     240.3 MB/s |     121.1 MB/s |    254 ns |    504 ns |   1.98x |
| repeat.json            | 11.1 KB |     936.7 MB/s |     399.5 MB/s |  11.6 µs |  27.1 µs |   2.34x |
| truenull.json          | 11.7 KB |     885.8 MB/s |     175.1 MB/s |  12.9 µs |  65.4 µs |   5.06x |
| twitter_timeline.json  | 41.2 KB |     818.8 MB/s |     277.0 MB/s |  49.2 µs | 145.4 µs |   2.96x |

### Encode

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     526.2 MB/s |     526.2 MB/s |    145 ns |    145 ns |   1.00x |
| demo.json              |  387 B |   1,222.1 MB/s |   1,085.5 MB/s |    302 ns |    340 ns |   1.13x |
| flatadversarial.json   |   64 B |     383.9 MB/s |     253.3 MB/s |    159 ns |    241 ns |   1.52x |
| repeat.json            | 11.1 KB |   1,401.6 MB/s |     822.9 MB/s |   7.7 µs |  13.2 µs |   1.70x |
| truenull.json          | 11.7 KB |   1,805.9 MB/s |   1,222.8 MB/s |   6.3 µs |   9.4 µs |   1.48x |
| twitter_timeline.json  | 41.2 KB |   1,193.2 MB/s |     897.0 MB/s |  33.8 µs |  44.9 µs |   1.33x |

### Validate

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     573.6 MB/s |     157.3 MB/s |    133 ns |    485 ns |   3.65x |
| demo.json              |  387 B |   1,464.6 MB/s |     338.3 MB/s |    252 ns |   1.1 µs |   4.33x |
| flatadversarial.json   |   64 B |     469.5 MB/s |     131.3 MB/s |    130 ns |    465 ns |   3.58x |
| repeat.json            | 11.1 KB |   1,639.7 MB/s |     447.8 MB/s |   6.6 µs |  24.2 µs |   3.66x |
| truenull.json          | 11.7 KB |   1,659.3 MB/s |     199.0 MB/s |   6.9 µs |  57.5 µs |   8.34x |
| twitter_timeline.json  | 41.2 KB |   2,203.6 MB/s |     313.9 MB/s |  18.3 µs | 128.3 µs |   7.02x |

## Memory peak (single-call delta)

Lower is better. The ratio is `fastjson / ext-json` peak heap; values **above 1.0 mean fastjson uses more memory**. This is the expected price of yyjson's two-stage model (build a doc, then walk into zvals or write a string), versus ext/json's streaming parser/writer that emits results directly.

### Decode (objects)

| File                   |    Size |     fastjson |     ext/json |  fast/ext |
|------------------------|---------|--------------|--------------|-----------|
| apache_builds.json     | 124.3 KB |     860.0 KB |     603.8 KB |     1.42x |
| canada.json            | 2.15 MB |     20.80 MB |     12.92 MB |     1.61x |
| citm_catalog.json      | 1.65 MB |      9.58 MB |      5.82 MB |     1.65x |
| github_events.json     | 63.6 KB |     320.8 KB |     192.5 KB |     1.67x |
| gsoc-2018.json         | 3.17 MB |     11.89 MB |      5.54 MB |     2.15x |
| instruments.json       | 215.2 KB |      1.28 MB |     871.6 KB |     1.51x |
| marine_ik.json         | 2.85 MB |     24.05 MB |     14.80 MB |     1.62x |
| mesh.json              | 706.6 KB |      5.05 MB |      2.52 MB |     2.01x |
| mesh.pretty.json       | 1.50 MB |      5.53 MB |      2.52 MB |     2.20x |
| numbers.json           | 146.6 KB |     800.1 KB |     260.1 KB |     3.08x |
| random.json            | 498.5 KB |      5.11 MB |      3.32 MB |     1.54x |
| adversarial.json       |   80 B |       1.3 KB |        952 B |     1.44x |
| demo.json              |  387 B |       2.8 KB |       1.9 KB |     1.50x |
| flatadversarial.json   |   64 B |       1.1 KB |        760 B |     1.53x |
| repeat.json            | 11.1 KB |      79.4 KB |      55.4 KB |     1.43x |
| truenull.json          | 11.7 KB |      80.1 KB |      36.1 KB |     2.22x |
| twitter_timeline.json  | 41.2 KB |     338.1 KB |     179.6 KB |     1.88x |
| stringifiedphp.json    | 139.9 KB |     276.1 KB |     136.0 KB |     2.03x |
| twitter.json           | 616.7 KB |      3.20 MB |      1.95 MB |     1.64x |
| twitterescaped.json    | 549.2 KB |      3.96 MB |      1.95 MB |     2.04x |
| update-center.json     | 520.7 KB |      4.65 MB |      2.75 MB |     1.69x |

### Decode (assoc arrays)

| File                   |    Size |     fastjson |     ext/json |  fast/ext |
|------------------------|---------|--------------|--------------|-----------|
| apache_builds.json     | 124.3 KB |     825.4 KB |     569.2 KB |     1.45x |
| canada.json            | 2.15 MB |     20.80 MB |     12.92 MB |     1.61x |
| citm_catalog.json      | 1.65 MB |      9.16 MB |      5.39 MB |     1.70x |
| github_events.json     | 63.6 KB |     313.8 KB |     185.4 KB |     1.69x |
| gsoc-2018.json         | 3.17 MB |     11.66 MB |      5.31 MB |     2.20x |
| instruments.json       | 215.2 KB |      1.25 MB |     832.1 KB |     1.53x |
| marine_ik.json         | 2.85 MB |     23.68 MB |     14.43 MB |     1.64x |
| mesh.json              | 706.6 KB |      5.05 MB |      2.52 MB |     2.01x |
| mesh.pretty.json       | 1.50 MB |      5.53 MB |      2.52 MB |     2.20x |
| numbers.json           | 146.6 KB |     800.1 KB |     260.1 KB |     3.08x |
| random.json            | 498.5 KB |      4.95 MB |      3.17 MB |     1.57x |
| adversarial.json       |   80 B |       1.3 KB |        912 B |     1.46x |
| demo.json              |  387 B |       2.7 KB |       1.8 KB |     1.54x |
| flatadversarial.json   |   64 B |       1.1 KB |        720 B |     1.56x |
| repeat.json            | 11.1 KB |      75.4 KB |      51.4 KB |     1.47x |
| truenull.json          | 11.7 KB |      80.1 KB |      36.1 KB |     2.22x |
| twitter_timeline.json  | 41.2 KB |     335.0 KB |     176.6 KB |     1.90x |
| stringifiedphp.json    | 139.9 KB |     276.1 KB |     136.0 KB |     2.03x |
| twitter.json           | 616.7 KB |      3.15 MB |      1.90 MB |     1.66x |
| twitterescaped.json    | 549.2 KB |      3.92 MB |      1.90 MB |     2.06x |
| update-center.json     | 520.7 KB |      4.58 MB |      2.68 MB |     1.71x |

### Encode

| File                   |    Size |     fastjson |     ext/json |  fast/ext |
|------------------------|---------|--------------|--------------|-----------|
| apache_builds.json     | 124.3 KB |     100.0 KB |     100.0 KB |     1.00x |
| canada.json            | 2.15 MB |      2.00 MB |      2.00 MB |     1.00x |
| citm_catalog.json      | 1.65 MB |     492.0 KB |     492.0 KB |     1.00x |
| github_events.json     | 63.6 KB |      56.0 KB |      56.0 KB |     1.00x |
| gsoc-2018.json         | 3.17 MB |      2.95 MB |      2.94 MB |     1.00x |
| instruments.json       | 215.2 KB |     108.0 KB |     108.0 KB |     1.00x |
| marine_ik.json         | 2.85 MB |      1.75 MB |      1.75 MB |     1.00x |
| mesh.json              | 706.6 KB |     628.0 KB |     628.0 KB |     1.00x |
| mesh.pretty.json       | 1.50 MB |     628.0 KB |     628.0 KB |     1.00x |
| numbers.json           | 146.6 KB |     148.0 KB |     148.0 KB |     1.00x |
| random.json            | 498.5 KB |     656.0 KB |     656.0 KB |     1.00x |
| adversarial.json       |   80 B |       4.0 KB |        336 B |    12.19x |
| demo.json              |  387 B |       4.0 KB |        256 B |    16.00x |
| flatadversarial.json   |   64 B |       4.0 KB |        352 B |    11.64x |
| repeat.json            | 11.1 KB |      12.0 KB |      12.0 KB |     1.00x |
| truenull.json          | 11.7 KB |      12.0 KB |      12.0 KB |     1.00x |
| twitter_timeline.json  | 41.2 KB |      44.0 KB |      44.0 KB |     1.00x |
| stringifiedphp.json    | 139.9 KB |     808.0 KB |     140.0 KB |     5.77x |
| twitter.json           | 616.7 KB |     556.0 KB |     556.0 KB |     1.00x |
| twitterescaped.json    | 549.2 KB |     556.0 KB |     556.0 KB |     1.00x |
| update-center.json     | 520.7 KB |     536.0 KB |     532.0 KB |     1.01x |

### Validate

| File                   |    Size |     fastjson |     ext/json |  fast/ext |
|------------------------|---------|--------------|--------------|-----------|
| apache_builds.json     | 124.3 KB |     128.1 KB |        512 B |   256.13x |
| canada.json            | 2.15 MB |      2.15 MB |         48 B | 46,935.17x |
| citm_catalog.json      | 1.65 MB |      1.65 MB |         80 B | 21,607.20x |
| github_events.json     | 63.6 KB |      64.1 KB |       8.0 KB |     8.01x |
| gsoc-2018.json         | 3.17 MB |      3.18 MB |       3.0 KB | 1,084.03x |
| instruments.json       | 215.2 KB |     216.1 KB |         64 B | 3,457.00x |
| marine_ik.json         | 2.85 MB |      2.85 MB |         64 B | 46,657.38x |
| mesh.json              | 706.6 KB |     708.1 KB |         40 B | 18,126.40x |
| mesh.pretty.json       | 1.50 MB |      1.51 MB |         40 B | 39,528.00x |
| numbers.json           | 146.6 KB |     148.1 KB |          0 B |       ∞ |
| random.json            | 498.5 KB |     500.1 KB |         64 B | 8,001.00x |
| adversarial.json       |   80 B |        160 B |         40 B |     4.00x |
| demo.json              |  387 B |        512 B |         64 B |     8.00x |
| flatadversarial.json   |   64 B |        144 B |         32 B |     4.50x |
| repeat.json            | 11.1 KB |      12.1 KB |         64 B |   193.00x |
| truenull.json          | 11.7 KB |      12.1 KB |          0 B |       ∞ |
| twitter_timeline.json  | 41.2 KB |      44.1 KB |        256 B |   176.25x |
| stringifiedphp.json    | 139.9 KB |     140.1 KB |     136.0 KB |     1.03x |
| twitter.json           | 616.7 KB |     620.1 KB |        512 B | 1,240.13x |
| twitterescaped.json    | 549.2 KB |     552.1 KB |        512 B | 1,104.13x |
| update-center.json     | 520.7 KB |     524.1 KB |       1.3 KB |   419.25x |

## Aggregate (sum across all files)

### Throughput, large corpus

| Operation              |    Bytes |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| Decode (objects)       | 14.81 MB |     427.5 MB/s |     165.1 MB/s |   2.59x |
| Decode (assoc arrays)  | 14.81 MB |     410.1 MB/s |     171.5 MB/s |   2.39x |
| Encode                 | 14.81 MB |     748.0 MB/s |     134.6 MB/s |   5.56x |
| Validate               | 14.81 MB |     994.2 MB/s |     197.3 MB/s |   5.04x |

### Throughput, small corpus

| Operation              |    Bytes |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| Decode (objects)       | 64.6 KB |     764.3 MB/s |     250.5 MB/s |   3.05x |
| Decode (assoc arrays)  | 64.6 KB |     843.7 MB/s |     262.5 MB/s |   3.21x |
| Encode                 | 64.6 KB |   1,302.2 MB/s |     925.3 MB/s |   1.41x |
| Validate               | 64.6 KB |   1,952.5 MB/s |     297.4 MB/s |   6.57x |

### Memory peak (across all files)

| Operation              |     fastjson |     ext/json |  fast/ext |
|------------------------|--------------|--------------|-----------|
| Decode (objects)       |     97.81 MB |     56.37 MB |     1.74x |
| Decode (assoc arrays)  |     96.38 MB |     54.94 MB |     1.75x |
| Encode                 |     11.92 MB |     11.24 MB |     1.06x |
| Validate               |     14.91 MB |     150.6 KB |   101.40x |

_Throughput: higher is better; speedup = ext/json time / fastjson time. Memory: lower is better; ratio = fastjson peak / ext/json peak (>1 means fastjson uses more)._
