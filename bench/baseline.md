# fastjson benchmark

- PHP 8.4.22-dev
- fastjson 0.4.0 (yyjson 0.12.0)
- ext/json 8.4.22-dev
- 300 iterations per case (slowest 10% dropped)
- CPU: 13th Gen Intel(R) Core(TM) i9-13950HX

## Throughput, large corpus

### Decode (objects)

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |     980.1 MB/s |     367.1 MB/s |   2.67x |
| canada.json            | 2.15 MB |     386.0 MB/s |      94.9 MB/s |   4.07x |
| citm_catalog.json      | 1.65 MB |   1,065.0 MB/s |     445.5 MB/s |   2.39x |
| github_events.json     | 63.6 KB |   1,259.6 MB/s |     413.4 MB/s |   3.05x |
| gsoc-2018.json         | 3.17 MB |   1,043.7 MB/s |     335.4 MB/s |   3.11x |
| instruments.json       | 215.2 KB |     917.6 MB/s |     345.3 MB/s |   2.66x |
| marine_ik.json         | 2.85 MB |     297.3 MB/s |     179.9 MB/s |   1.65x |
| mesh.json              | 706.6 KB |     482.7 MB/s |     187.8 MB/s |   2.57x |
| mesh.pretty.json       | 1.50 MB |     653.5 MB/s |     241.7 MB/s |   2.70x |
| numbers.json           | 146.6 KB |     919.0 MB/s |     224.6 MB/s |   4.09x |
| random.json            | 498.5 KB |     465.1 MB/s |     246.4 MB/s |   1.89x |
| stringifiedphp.json    | 139.9 KB |   2,487.4 MB/s |     338.0 MB/s |   7.36x |
| twitter.json           | 616.7 KB |     941.6 MB/s |     392.8 MB/s |   2.40x |
| twitterescaped.json    | 549.2 KB |     779.4 MB/s |     295.5 MB/s |   2.64x |
| update-center.json     | 520.7 KB |     556.9 MB/s |     258.9 MB/s |   2.15x |

### Decode (assoc arrays)

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,115.3 MB/s |     395.7 MB/s |   2.82x |
| canada.json            | 2.15 MB |     377.9 MB/s |      95.3 MB/s |   3.97x |
| citm_catalog.json      | 1.65 MB |     728.4 MB/s |     470.2 MB/s |   1.55x |
| github_events.json     | 63.6 KB |   1,365.5 MB/s |     410.3 MB/s |   3.33x |
| gsoc-2018.json         | 3.17 MB |     985.6 MB/s |     315.8 MB/s |   3.12x |
| instruments.json       | 215.2 KB |   1,024.0 MB/s |     377.2 MB/s |   2.72x |
| marine_ik.json         | 2.85 MB |     294.4 MB/s |     187.4 MB/s |   1.57x |
| mesh.json              | 706.6 KB |     466.1 MB/s |     188.8 MB/s |   2.47x |
| mesh.pretty.json       | 1.50 MB |     678.7 MB/s |     250.2 MB/s |   2.71x |
| numbers.json           | 146.6 KB |     927.6 MB/s |     235.1 MB/s |   3.95x |
| random.json            | 498.5 KB |     527.4 MB/s |     255.1 MB/s |   2.07x |
| stringifiedphp.json    | 139.9 KB |   2,643.4 MB/s |     337.8 MB/s |   7.82x |
| twitter.json           | 616.7 KB |   1,092.6 MB/s |     409.5 MB/s |   2.67x |
| twitterescaped.json    | 549.2 KB |     880.9 MB/s |     302.3 MB/s |   2.91x |
| update-center.json     | 520.7 KB |     613.3 MB/s |     269.1 MB/s |   2.28x |

### Encode

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,458.5 MB/s |   1,093.4 MB/s |   1.33x |
| canada.json            | 2.15 MB |     670.9 MB/s |      55.6 MB/s |  12.07x |
| citm_catalog.json      | 1.65 MB |   2,708.7 MB/s |   2,349.4 MB/s |   1.15x |
| github_events.json     | 63.6 KB |   1,951.6 MB/s |   1,243.1 MB/s |   1.57x |
| gsoc-2018.json         | 3.17 MB |   1,248.6 MB/s |     714.4 MB/s |   1.75x |
| instruments.json       | 215.2 KB |   1,901.9 MB/s |   1,751.8 MB/s |   1.09x |
| marine_ik.json         | 2.85 MB |     636.3 MB/s |     124.7 MB/s |   5.10x |
| mesh.json              | 706.6 KB |     735.7 MB/s |      82.2 MB/s |   8.95x |
| mesh.pretty.json       | 1.50 MB |   1,586.5 MB/s |     178.6 MB/s |   8.88x |
| numbers.json           | 146.6 KB |     649.2 MB/s |      50.8 MB/s |  12.77x |
| random.json            | 498.5 KB |     803.8 MB/s |     588.5 MB/s |   1.37x |
| stringifiedphp.json    | 139.9 KB |   2,870.4 MB/s |     722.5 MB/s |   3.97x |
| twitter.json           | 616.7 KB |   1,608.3 MB/s |   1,089.6 MB/s |   1.48x |
| twitterescaped.json    | 549.2 KB |   1,411.8 MB/s |     981.1 MB/s |   1.44x |
| update-center.json     | 520.7 KB |     991.8 MB/s |     777.6 MB/s |   1.28x |

### Validate

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   2,470.2 MB/s |     414.0 MB/s |   5.97x |
| canada.json            | 2.15 MB |     862.5 MB/s |     103.6 MB/s |   8.32x |
| citm_catalog.json      | 1.65 MB |   2,456.9 MB/s |     551.4 MB/s |   4.46x |
| github_events.json     | 63.6 KB |   2,876.6 MB/s |     476.2 MB/s |   6.04x |
| gsoc-2018.json         | 3.17 MB |   1,468.7 MB/s |     352.9 MB/s |   4.16x |
| instruments.json       | 215.2 KB |   2,141.7 MB/s |     436.7 MB/s |   4.90x |
| marine_ik.json         | 2.85 MB |     820.8 MB/s |     238.1 MB/s |   3.45x |
| mesh.json              | 706.6 KB |   1,155.2 MB/s |     212.0 MB/s |   5.45x |
| mesh.pretty.json       | 1.50 MB |   1,575.4 MB/s |     258.7 MB/s |   6.09x |
| numbers.json           | 146.6 KB |   1,347.0 MB/s |     237.3 MB/s |   5.68x |
| random.json            | 498.5 KB |   1,408.5 MB/s |     333.2 MB/s |   4.23x |
| stringifiedphp.json    | 139.9 KB |   2,699.1 MB/s |     334.3 MB/s |   8.07x |
| twitter.json           | 616.7 KB |   2,430.8 MB/s |     502.3 MB/s |   4.84x |
| twitterescaped.json    | 549.2 KB |   2,309.2 MB/s |     354.7 MB/s |   6.51x |
| update-center.json     | 520.7 KB |   1,908.0 MB/s |     306.0 MB/s |   6.23x |

## Throughput, small corpus

### Decode (objects)

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     315.3 MB/s |     168.4 MB/s |    242 ns |    453 ns |   1.87x |
| demo.json              |  387 B |     728.0 MB/s |     307.8 MB/s |    507 ns |   1.2 µs |   2.36x |
| flatadversarial.json   |   64 B |     246.1 MB/s |     131.0 MB/s |    248 ns |    466 ns |   1.88x |
| repeat.json            | 11.1 KB |     883.7 MB/s |     433.9 MB/s |  12.3 µs |  25.0 µs |   2.04x |
| truenull.json          | 11.7 KB |     845.1 MB/s |     207.8 MB/s |  13.5 µs |  55.1 µs |   4.07x |
| twitter_timeline.json  | 41.2 KB |     868.9 MB/s |     306.4 MB/s |  46.4 µs | 131.5 µs |   2.84x |

### Decode (assoc arrays)

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     336.1 MB/s |     175.8 MB/s |    227 ns |    434 ns |   1.91x |
| demo.json              |  387 B |     802.3 MB/s |     346.2 MB/s |    460 ns |   1.1 µs |   2.32x |
| flatadversarial.json   |   64 B |     266.5 MB/s |     138.1 MB/s |    229 ns |    442 ns |   1.93x |
| repeat.json            | 11.1 KB |   1,045.3 MB/s |     462.9 MB/s |  10.4 µs |  23.4 µs |   2.26x |
| truenull.json          | 11.7 KB |     808.1 MB/s |     199.4 MB/s |  14.2 µs |  57.4 µs |   4.05x |
| twitter_timeline.json  | 41.2 KB |     985.0 MB/s |     328.3 MB/s |  40.9 µs | 122.7 µs |   3.00x |

### Encode

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     596.0 MB/s |     610.4 MB/s |    128 ns |    125 ns |   0.98x |
| demo.json              |  387 B |   1,464.6 MB/s |   1,259.6 MB/s |    252 ns |    293 ns |   1.16x |
| flatadversarial.json   |   64 B |     436.0 MB/s |     297.7 MB/s |    140 ns |    205 ns |   1.46x |
| repeat.json            | 11.1 KB |   1,707.1 MB/s |     959.5 MB/s |   6.3 µs |  11.3 µs |   1.78x |
| truenull.json          | 11.7 KB |   2,169.5 MB/s |   1,457.1 MB/s |   5.3 µs |   7.9 µs |   1.49x |
| twitter_timeline.json  | 41.2 KB |   1,427.3 MB/s |   1,062.4 MB/s |  28.2 µs |  37.9 µs |   1.34x |

### Validate

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     652.1 MB/s |     159.6 MB/s |    117 ns |    478 ns |   4.09x |
| demo.json              |  387 B |   1,425.0 MB/s |     360.1 MB/s |    259 ns |   1.0 µs |   3.96x |
| flatadversarial.json   |   64 B |     554.9 MB/s |     161.0 MB/s |    110 ns |    379 ns |   3.45x |
| repeat.json            | 11.1 KB |   1,986.8 MB/s |     536.3 MB/s |   5.5 µs |  20.2 µs |   3.70x |
| truenull.json          | 11.7 KB |   2,314.3 MB/s |     232.0 MB/s |   4.9 µs |  49.3 µs |   9.97x |
| twitter_timeline.json  | 41.2 KB |   2,688.1 MB/s |     381.8 MB/s |  15.0 µs | 105.5 µs |   7.04x |

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
| twitter_timeline.json  | 41.2 KB |     338.5 KB |     179.6 KB |     1.89x |
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
| Decode (objects)       | 14.81 MB |     541.7 MB/s |     208.9 MB/s |   2.59x |
| Decode (assoc arrays)  | 14.81 MB |     529.1 MB/s |     211.6 MB/s |   2.50x |
| Encode                 | 14.81 MB |     982.8 MB/s |     165.9 MB/s |   5.92x |
| Validate               | 14.81 MB |   1,260.1 MB/s |     241.0 MB/s |   5.23x |

### Throughput, small corpus

| Operation              |    Bytes |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| Decode (objects)       | 64.6 KB |     862.1 MB/s |     295.2 MB/s |   2.92x |
| Decode (assoc arrays)  | 64.6 KB |     950.7 MB/s |     307.0 MB/s |   3.10x |
| Encode                 | 64.6 KB |   1,562.5 MB/s |   1,093.3 MB/s |   1.43x |
| Validate               | 64.6 KB |   2,437.9 MB/s |     356.5 MB/s |   6.84x |

### Memory peak (across all files)

| Operation              |     fastjson |     ext/json |  fast/ext |
|------------------------|--------------|--------------|-----------|
| Decode (objects)       |     97.81 MB |     56.37 MB |     1.74x |
| Decode (assoc arrays)  |     96.38 MB |     54.94 MB |     1.75x |
| Encode                 |     11.92 MB |     11.24 MB |     1.06x |
| Validate               |     14.91 MB |     150.6 KB |   101.40x |

_Throughput: higher is better; speedup = ext/json time / fastjson time. Memory: lower is better; ratio = fastjson peak / ext/json peak (>1 means fastjson uses more)._
