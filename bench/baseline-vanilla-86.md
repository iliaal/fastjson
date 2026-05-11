# fastjson benchmark

- PHP 8.6.0-dev
- fastjson 0.1.0 (yyjson 0.12.0)
- ext/json 8.6.0-dev
- simdjson 4.0.1dev (decode + validate only)
- 200 iterations per case (slowest 10% dropped)
- CPU: 13th Gen Intel(R) Core(TM) i9-13950HX

## Throughput, large corpus

### Decode (objects)

| File                   |     Size |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,046.9 MB/s |     363.7 MB/s |     896.8 MB/s |   2.88x |
| canada.json            | 2.15 MB |     434.3 MB/s |      97.9 MB/s |     609.2 MB/s |   4.43x |
| citm_catalog.json      | 1.65 MB |   1,153.4 MB/s |     427.5 MB/s |   1,111.6 MB/s |   2.70x |
| github_events.json     | 63.6 KB |   1,259.5 MB/s |     381.6 MB/s |   1,261.7 MB/s |   3.30x |
| gsoc-2018.json         | 3.17 MB |   1,065.7 MB/s |     345.4 MB/s |   1,846.9 MB/s |   3.09x |
| instruments.json       | 215.2 KB |     908.8 MB/s |     352.8 MB/s |     862.2 MB/s |   2.58x |
| marine_ik.json         | 2.85 MB |     302.3 MB/s |     177.7 MB/s |     399.9 MB/s |   1.70x |
| mesh.json              | 706.6 KB |     478.3 MB/s |     186.8 MB/s |     674.9 MB/s |   2.56x |
| mesh.pretty.json       | 1.50 MB |     842.3 MB/s |     253.5 MB/s |   1,217.0 MB/s |   3.32x |
| numbers.json           | 146.6 KB |   1,000.7 MB/s |     239.6 MB/s |     959.4 MB/s |   4.18x |
| random.json            | 498.5 KB |     499.7 MB/s |     241.6 MB/s |     504.1 MB/s |   2.07x |
| stringifiedphp.json    | 139.9 KB |   2,668.5 MB/s |     350.0 MB/s |   3,212.5 MB/s |   7.62x |
| twitter.json           | 616.7 KB |     981.0 MB/s |     394.2 MB/s |   1,020.3 MB/s |   2.49x |
| twitterescaped.json    | 549.2 KB |     841.1 MB/s |     305.4 MB/s |     683.4 MB/s |   2.75x |
| update-center.json     | 520.7 KB |     616.9 MB/s |     259.9 MB/s |     619.7 MB/s |   2.37x |

### Decode (assoc arrays)

| File                   |     Size |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,044.4 MB/s |     366.1 MB/s |   1,107.1 MB/s |   2.85x |
| canada.json            | 2.15 MB |     428.2 MB/s |      96.3 MB/s |     584.2 MB/s |   4.45x |
| citm_catalog.json      | 1.65 MB |     784.8 MB/s |     461.2 MB/s |   1,254.5 MB/s |   1.70x |
| github_events.json     | 63.6 KB |   1,413.8 MB/s |     403.8 MB/s |   1,446.7 MB/s |   3.50x |
| gsoc-2018.json         | 3.17 MB |   1,118.3 MB/s |     331.8 MB/s |   1,963.2 MB/s |   3.37x |
| instruments.json       | 215.2 KB |     923.7 MB/s |     363.9 MB/s |   1,038.6 MB/s |   2.54x |
| marine_ik.json         | 2.85 MB |     288.3 MB/s |     186.1 MB/s |     426.3 MB/s |   1.55x |
| mesh.json              | 706.6 KB |     436.6 MB/s |     193.5 MB/s |     693.9 MB/s |   2.26x |
| mesh.pretty.json       | 1.50 MB |     705.6 MB/s |     252.1 MB/s |   1,261.3 MB/s |   2.80x |
| numbers.json           | 146.6 KB |   1,013.3 MB/s |     246.1 MB/s |   1,020.8 MB/s |   4.12x |
| random.json            | 498.5 KB |     559.1 MB/s |     250.7 MB/s |     564.0 MB/s |   2.23x |
| stringifiedphp.json    | 139.9 KB |   2,643.8 MB/s |     345.1 MB/s |   3,126.0 MB/s |   7.66x |
| twitter.json           | 616.7 KB |   1,103.6 MB/s |     410.4 MB/s |   1,066.9 MB/s |   2.69x |
| twitterescaped.json    | 549.2 KB |     893.0 MB/s |     313.6 MB/s |     747.1 MB/s |   2.85x |
| update-center.json     | 520.7 KB |     668.4 MB/s |     274.5 MB/s |     703.7 MB/s |   2.43x |

### Encode

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,643.4 MB/s |   1,097.7 MB/s |   1.50x |
| canada.json            | 2.15 MB |     716.7 MB/s |      60.5 MB/s |  11.84x |
| citm_catalog.json      | 1.65 MB |   3,139.4 MB/s |   2,280.0 MB/s |   1.38x |
| github_events.json     | 63.6 KB |   2,140.8 MB/s |   1,186.7 MB/s |   1.80x |
| gsoc-2018.json         | 3.17 MB |   1,310.3 MB/s |     716.3 MB/s |   1.83x |
| instruments.json       | 215.2 KB |   2,182.8 MB/s |   1,760.2 MB/s |   1.24x |
| marine_ik.json         | 2.85 MB |     657.0 MB/s |     131.5 MB/s |   5.00x |
| mesh.json              | 706.6 KB |     744.8 MB/s |      89.7 MB/s |   8.30x |
| mesh.pretty.json       | 1.50 MB |   1,690.3 MB/s |     199.1 MB/s |   8.49x |
| numbers.json           | 146.6 KB |     667.1 MB/s |      57.2 MB/s |  11.65x |
| random.json            | 498.5 KB |     831.3 MB/s |     591.1 MB/s |   1.41x |
| stringifiedphp.json    | 139.9 KB |   2,920.6 MB/s |     725.0 MB/s |   4.03x |
| twitter.json           | 616.7 KB |   1,691.8 MB/s |   1,065.9 MB/s |   1.59x |
| twitterescaped.json    | 549.2 KB |   1,469.0 MB/s |     949.4 MB/s |   1.55x |
| update-center.json     | 520.7 KB |   1,075.6 MB/s |     804.4 MB/s |   1.34x |

### Validate

| File                   |     Size |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   2,290.9 MB/s |     385.2 MB/s |   3,957.6 MB/s |   5.95x |
| canada.json            | 2.15 MB |     905.9 MB/s |     106.0 MB/s |   1,258.5 MB/s |   8.55x |
| citm_catalog.json      | 1.65 MB |   2,407.3 MB/s |     564.9 MB/s |   4,037.7 MB/s |   4.26x |
| github_events.json     | 63.6 KB |   3,014.5 MB/s |     444.3 MB/s |   4,720.3 MB/s |   6.79x |
| gsoc-2018.json         | 3.17 MB |   1,549.9 MB/s |     372.7 MB/s |   4,250.4 MB/s |   4.16x |
| instruments.json       | 215.2 KB |   2,128.6 MB/s |     437.3 MB/s |   3,558.8 MB/s |   4.87x |
| marine_ik.json         | 2.85 MB |     894.5 MB/s |     234.9 MB/s |   1,224.9 MB/s |   3.81x |
| mesh.json              | 706.6 KB |   1,291.8 MB/s |     207.0 MB/s |   1,268.5 MB/s |   6.24x |
| mesh.pretty.json       | 1.50 MB |   1,695.8 MB/s |     261.6 MB/s |   2,033.4 MB/s |   6.48x |
| numbers.json           | 146.6 KB |   1,482.2 MB/s |     250.9 MB/s |   1,584.5 MB/s |   5.91x |
| random.json            | 498.5 KB |   1,515.4 MB/s |     309.6 MB/s |   2,537.8 MB/s |   4.90x |
| stringifiedphp.json    | 139.9 KB |   2,848.6 MB/s |     360.2 MB/s |   3,337.4 MB/s |   7.91x |
| twitter.json           | 616.7 KB |   2,735.7 MB/s |     482.2 MB/s |   3,907.6 MB/s |   5.67x |
| twitterescaped.json    | 549.2 KB |   2,375.8 MB/s |     353.2 MB/s |   1,735.9 MB/s |   6.73x |
| update-center.json     | 520.7 KB |   2,149.6 MB/s |     315.6 MB/s |   3,078.1 MB/s |   6.81x |

## Throughput, small corpus

### Decode (objects)

| File                   |    Size |     fastjson |     ext/json |     simdjson | fast/call |  ext/call |   sj/call | speedup |
|------------------------|---------|--------------|--------------|--------------|-----------|-----------|-----------|---------|
| adversarial.json       |   80 B |     330.3 MB/s |     164.8 MB/s |     316.6 MB/s |    231 ns |    463 ns |    241 ns |   2.00x |
| demo.json              |  387 B |     781.9 MB/s |     322.1 MB/s |     686.0 MB/s |    472 ns |   1.1 µs |    538 ns |   2.43x |
| flatadversarial.json   |   64 B |     246.1 MB/s |     126.1 MB/s |     227.7 MB/s |    248 ns |    484 ns |    268 ns |   1.95x |
| repeat.json            | 11.1 KB |     947.4 MB/s |     432.4 MB/s |   1,009.4 MB/s |  11.4 µs |  25.0 µs |  10.7 µs |   2.19x |
| truenull.json          | 11.7 KB |     888.8 MB/s |     199.7 MB/s |     804.8 MB/s |  12.9 µs |  57.3 µs |  14.2 µs |   4.45x |
| twitter_timeline.json  | 41.2 KB |     910.4 MB/s |     304.0 MB/s |     816.2 MB/s |  44.2 µs | 132.5 µs |  49.3 µs |   2.99x |

### Decode (assoc arrays)

| File                   |    Size |     fastjson |     ext/json |     simdjson | fast/call |  ext/call |   sj/call | speedup |
|------------------------|---------|--------------|--------------|--------------|-----------|-----------|-----------|---------|
| adversarial.json       |   80 B |     391.3 MB/s |     176.6 MB/s |     361.6 MB/s |    195 ns |    432 ns |    211 ns |   2.22x |
| demo.json              |  387 B |     934.4 MB/s |     362.9 MB/s |     860.3 MB/s |    395 ns |   1.0 µs |    429 ns |   2.57x |
| flatadversarial.json   |   64 B |     313.0 MB/s |     145.7 MB/s |     292.0 MB/s |    195 ns |    419 ns |    209 ns |   2.15x |
| repeat.json            | 11.1 KB |   1,152.4 MB/s |     480.3 MB/s |   1,273.8 MB/s |   9.4 µs |  22.6 µs |   8.5 µs |   2.40x |
| truenull.json          | 11.7 KB |     888.4 MB/s |     199.7 MB/s |     789.8 MB/s |  12.9 µs |  57.3 µs |  14.5 µs |   4.45x |
| twitter_timeline.json  | 41.2 KB |   1,031.3 MB/s |     330.7 MB/s |     950.6 MB/s |  39.1 µs | 121.8 µs |  42.4 µs |   3.12x |

### Encode

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     630.5 MB/s |     687.3 MB/s |    121 ns |    111 ns |   0.92x |
| demo.json              |  387 B |   1,557.3 MB/s |   1,318.1 MB/s |    237 ns |    280 ns |   1.18x |
| flatadversarial.json   |   64 B |     458.9 MB/s |     342.9 MB/s |    133 ns |    178 ns |   1.34x |
| repeat.json            | 11.1 KB |   1,745.1 MB/s |     990.2 MB/s |   6.2 µs |  10.9 µs |   1.76x |
| truenull.json          | 11.7 KB |   2,356.2 MB/s |   2,071.0 MB/s |   4.9 µs |   5.5 µs |   1.14x |
| twitter_timeline.json  | 41.2 KB |   1,706.8 MB/s |   1,091.1 MB/s |  23.6 µs |  36.9 µs |   1.56x |

### Validate

| File                   |    Size |     fastjson |     ext/json |     simdjson | fast/call |  ext/call |   sj/call | speedup |
|------------------------|---------|--------------|--------------|--------------|-----------|-----------|-----------|---------|
| adversarial.json       |   80 B |     693.6 MB/s |     195.1 MB/s |     726.6 MB/s |    110 ns |    391 ns |    105 ns |   3.55x |
| demo.json              |  387 B |   1,670.0 MB/s |     341.4 MB/s |   1,716.6 MB/s |    221 ns |   1.1 µs |    215 ns |   4.89x |
| flatadversarial.json   |   64 B |     492.2 MB/s |     156.5 MB/s |     581.3 MB/s |    124 ns |    390 ns |    105 ns |   3.15x |
| repeat.json            | 11.1 KB |   2,098.0 MB/s |     548.2 MB/s |   4,015.5 MB/s |   5.2 µs |  19.8 µs |   2.7 µs |   3.83x |
| truenull.json          | 11.7 KB |   2,595.6 MB/s |     222.3 MB/s |   2,445.3 MB/s |   4.4 µs |  51.5 µs |   4.7 µs |  11.68x |
| twitter_timeline.json  | 41.2 KB |   2,971.1 MB/s |     379.9 MB/s |   3,113.0 MB/s |  13.6 µs | 106.0 µs |  12.9 µs |   7.82x |

## Memory peak (single-call delta)

Lower is better. The ratio is `fastjson / ext-json` peak heap; values **above 1.0 mean fastjson uses more memory**. This is the expected price of yyjson's two-stage model (build a doc, then walk into zvals or write a string), versus ext/json's streaming parser/writer that emits results directly.

### Decode (objects)

| File                   |    Size |     fastjson |     ext/json |     simdjson |  fast/ext |
|------------------------|---------|--------------|--------------|--------------|-----------|
| apache_builds.json     | 124.3 KB |     860.0 KB |     603.8 KB |     603.8 KB |     1.42x |
| canada.json            | 2.15 MB |     20.80 MB |     12.92 MB |     12.92 MB |     1.61x |
| citm_catalog.json      | 1.65 MB |      9.58 MB |      5.82 MB |      5.82 MB |     1.65x |
| github_events.json     | 63.6 KB |     320.8 KB |     192.5 KB |     192.5 KB |     1.67x |
| gsoc-2018.json         | 3.17 MB |     11.89 MB |      5.54 MB |      5.54 MB |     2.15x |
| instruments.json       | 215.2 KB |      1.28 MB |     871.6 KB |     871.6 KB |     1.51x |
| marine_ik.json         | 2.85 MB |     24.05 MB |     14.80 MB |     14.80 MB |     1.62x |
| mesh.json              | 706.6 KB |      5.05 MB |      2.52 MB |      2.52 MB |     2.01x |
| mesh.pretty.json       | 1.50 MB |      5.53 MB |      2.52 MB |      2.52 MB |     2.20x |
| numbers.json           | 146.6 KB |     800.1 KB |     260.1 KB |     260.1 KB |     3.08x |
| random.json            | 498.5 KB |      5.11 MB |      3.32 MB |      3.32 MB |     1.54x |
| adversarial.json       |   80 B |       1.3 KB |        952 B |        920 B |     1.44x |
| demo.json              |  387 B |       2.8 KB |       1.9 KB |       1.9 KB |     1.50x |
| flatadversarial.json   |   64 B |       1.1 KB |        760 B |        728 B |     1.53x |
| repeat.json            | 11.1 KB |      79.4 KB |      55.4 KB |      55.4 KB |     1.43x |
| truenull.json          | 11.7 KB |      80.1 KB |      36.1 KB |      36.1 KB |     2.22x |
| twitter_timeline.json  | 41.2 KB |     338.5 KB |     179.6 KB |     179.3 KB |     1.89x |
| stringifiedphp.json    | 139.9 KB |     276.1 KB |     136.0 KB |     136.0 KB |     2.03x |
| twitter.json           | 616.7 KB |      3.20 MB |      1.95 MB |      1.95 MB |     1.64x |
| twitterescaped.json    | 549.2 KB |      3.96 MB |      1.95 MB |      1.95 MB |     2.04x |
| update-center.json     | 520.7 KB |      4.65 MB |      2.75 MB |      2.75 MB |     1.69x |

### Decode (assoc arrays)

| File                   |    Size |     fastjson |     ext/json |     simdjson |  fast/ext |
|------------------------|---------|--------------|--------------|--------------|-----------|
| apache_builds.json     | 124.3 KB |     825.4 KB |     569.2 KB |     569.2 KB |     1.45x |
| canada.json            | 2.15 MB |     20.80 MB |     12.92 MB |     12.92 MB |     1.61x |
| citm_catalog.json      | 1.65 MB |      9.16 MB |      5.39 MB |      5.39 MB |     1.70x |
| github_events.json     | 63.6 KB |     313.8 KB |     185.4 KB |     185.4 KB |     1.69x |
| gsoc-2018.json         | 3.17 MB |     11.66 MB |      5.31 MB |      5.31 MB |     2.20x |
| instruments.json       | 215.2 KB |      1.25 MB |     832.1 KB |     832.1 KB |     1.53x |
| marine_ik.json         | 2.85 MB |     23.68 MB |     14.43 MB |     14.43 MB |     1.64x |
| mesh.json              | 706.6 KB |      5.05 MB |      2.52 MB |      2.52 MB |     2.01x |
| mesh.pretty.json       | 1.50 MB |      5.53 MB |      2.52 MB |      2.52 MB |     2.20x |
| numbers.json           | 146.6 KB |     800.1 KB |     260.1 KB |     260.1 KB |     3.08x |
| random.json            | 498.5 KB |      4.95 MB |      3.17 MB |      3.17 MB |     1.57x |
| adversarial.json       |   80 B |       1.3 KB |        912 B |        880 B |     1.46x |
| demo.json              |  387 B |       2.7 KB |       1.8 KB |       1.8 KB |     1.54x |
| flatadversarial.json   |   64 B |       1.1 KB |        720 B |        688 B |     1.56x |
| repeat.json            | 11.1 KB |      75.4 KB |      51.4 KB |      51.4 KB |     1.47x |
| truenull.json          | 11.7 KB |      80.1 KB |      36.1 KB |      36.1 KB |     2.22x |
| twitter_timeline.json  | 41.2 KB |     335.0 KB |     176.6 KB |     176.3 KB |     1.90x |
| stringifiedphp.json    | 139.9 KB |     276.1 KB |     136.0 KB |     136.0 KB |     2.03x |
| twitter.json           | 616.7 KB |      3.15 MB |      1.90 MB |      1.90 MB |     1.66x |
| twitterescaped.json    | 549.2 KB |      3.92 MB |      1.90 MB |      1.90 MB |     2.06x |
| update-center.json     | 520.7 KB |      4.58 MB |      2.68 MB |      2.68 MB |     1.71x |

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

| File                   |    Size |     fastjson |     ext/json |     simdjson |  fast/ext |
|------------------------|---------|--------------|--------------|--------------|-----------|
| apache_builds.json     | 124.3 KB |     128.1 KB |        512 B |          0 B |   256.13x |
| canada.json            | 2.15 MB |      2.15 MB |         48 B |          0 B | 46,935.17x |
| citm_catalog.json      | 1.65 MB |      1.65 MB |         80 B |          0 B | 21,607.20x |
| github_events.json     | 63.6 KB |      64.1 KB |       8.0 KB |          0 B |     8.01x |
| gsoc-2018.json         | 3.17 MB |      3.18 MB |       3.0 KB |          0 B | 1,084.03x |
| instruments.json       | 215.2 KB |     216.1 KB |         64 B |          0 B | 3,457.00x |
| marine_ik.json         | 2.85 MB |      2.85 MB |         64 B |          0 B | 46,657.38x |
| mesh.json              | 706.6 KB |     708.1 KB |         40 B |          0 B | 18,126.40x |
| mesh.pretty.json       | 1.50 MB |      1.51 MB |         40 B |          0 B | 39,528.00x |
| numbers.json           | 146.6 KB |     148.1 KB |          0 B |          0 B |       ∞ |
| random.json            | 498.5 KB |     500.1 KB |         64 B |          0 B | 8,001.00x |
| adversarial.json       |   80 B |        160 B |         40 B |          0 B |     4.00x |
| demo.json              |  387 B |        512 B |         64 B |          0 B |     8.00x |
| flatadversarial.json   |   64 B |        144 B |         32 B |          0 B |     4.50x |
| repeat.json            | 11.1 KB |      12.1 KB |         64 B |          0 B |   193.00x |
| truenull.json          | 11.7 KB |      12.1 KB |          0 B |          0 B |       ∞ |
| twitter_timeline.json  | 41.2 KB |      44.1 KB |        256 B |          0 B |   176.25x |
| stringifiedphp.json    | 139.9 KB |     140.1 KB |     136.0 KB |          0 B |     1.03x |
| twitter.json           | 616.7 KB |     620.1 KB |        512 B |          0 B | 1,240.13x |
| twitterescaped.json    | 549.2 KB |     552.1 KB |        512 B |          0 B | 1,104.13x |
| update-center.json     | 520.7 KB |     524.1 KB |       1.3 KB |          0 B |   419.25x |

## Aggregate (sum across all files)

### Throughput, large corpus

| Operation              |    Bytes |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| Decode (objects)       | 14.81 MB |     578.0 MB/s |     211.8 MB/s |     745.2 MB/s |   2.73x |
| Decode (assoc arrays)  | 14.81 MB |     550.9 MB/s |     213.9 MB/s |     785.0 MB/s |   2.58x |
| Encode                 | 14.81 MB |   1,033.9 MB/s |     178.3 MB/s |             - |   5.80x |
| Validate               | 14.81 MB |   1,343.8 MB/s |     244.0 MB/s |   1,969.7 MB/s |   5.51x |

### Throughput, small corpus

| Operation              |    Bytes |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| Decode (objects)       | 64.6 KB |     907.3 MB/s |     290.6 MB/s |     837.0 MB/s |   3.12x |
| Decode (assoc arrays)  | 64.6 KB |   1,015.1 MB/s |     309.8 MB/s |     952.4 MB/s |   3.28x |
| Encode                 | 64.6 KB |   1,793.8 MB/s |   1,168.9 MB/s |             - |   1.53x |
| Validate               | 64.6 KB |   2,673.9 MB/s |     352.0 MB/s |   3,040.4 MB/s |   7.60x |

### Memory peak (across all files)

| Operation              |     fastjson |     ext/json |     simdjson |  fast/ext |
|------------------------|--------------|--------------|--------------|-----------|
| Decode (objects)       |     97.81 MB |     56.37 MB |     56.36 MB |     1.74x |
| Decode (assoc arrays)  |     96.38 MB |     54.94 MB |     54.93 MB |     1.75x |
| Encode                 |     11.92 MB |     11.24 MB |            - |     1.06x |
| Validate               |     14.91 MB |     150.6 KB |          0 B |   101.40x |

_Throughput: higher is better; speedup = ext/json time / fastjson time. Memory: lower is better; ratio = fastjson peak / ext/json peak (>1 means fastjson uses more)._
