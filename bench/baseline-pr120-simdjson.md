# fastjson benchmark

- PHP 8.6.0-dev
- fastjson 0.1.0 (yyjson 0.12.0)
- ext/json 8.6.0-dev
- simdjson 4.0.1dev (decode + validate only)
- 50 iterations per case (slowest 10% dropped)
- CPU: 13th Gen Intel(R) Core(TM) i9-13950HX

## Throughput, large corpus

### Decode (objects)

| File                   |     Size |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |     905.2 MB/s |     317.2 MB/s |     907.0 MB/s |   2.85x |
| canada.json            | 2.15 MB |     374.7 MB/s |      95.5 MB/s |     567.0 MB/s |   3.92x |
| citm_catalog.json      | 1.65 MB |   1,003.1 MB/s |     400.5 MB/s |   1,053.5 MB/s |   2.50x |
| github_events.json     | 63.6 KB |   1,196.8 MB/s |     335.8 MB/s |   1,186.9 MB/s |   3.56x |
| gsoc-2018.json         | 3.17 MB |   1,056.9 MB/s |     299.0 MB/s |   1,643.6 MB/s |   3.53x |
| instruments.json       | 215.2 KB |     735.9 MB/s |     295.5 MB/s |     772.2 MB/s |   2.49x |
| marine_ik.json         | 2.85 MB |     292.5 MB/s |     166.1 MB/s |     409.6 MB/s |   1.76x |
| mesh.json              | 706.6 KB |     495.2 MB/s |     177.5 MB/s |     663.2 MB/s |   2.79x |
| mesh.pretty.json       | 1.50 MB |     675.8 MB/s |     235.2 MB/s |   1,168.3 MB/s |   2.87x |
| numbers.json           | 146.6 KB |     946.0 MB/s |     215.9 MB/s |     982.3 MB/s |   4.38x |
| random.json            | 498.5 KB |     479.0 MB/s |     223.4 MB/s |     481.9 MB/s |   2.14x |
| stringifiedphp.json    | 139.9 KB |   2,398.4 MB/s |     299.5 MB/s |   3,102.6 MB/s |   8.01x |
| twitter.json           | 616.7 KB |     890.1 MB/s |     348.8 MB/s |     859.1 MB/s |   2.55x |
| twitterescaped.json    | 549.2 KB |     653.8 MB/s |     232.0 MB/s |     562.9 MB/s |   2.82x |
| update-center.json     | 520.7 KB |     503.9 MB/s |     221.1 MB/s |     594.6 MB/s |   2.28x |

### Decode (assoc arrays)

| File                   |     Size |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,034.4 MB/s |     330.4 MB/s |   1,069.7 MB/s |   3.13x |
| canada.json            | 2.15 MB |     379.9 MB/s |      92.9 MB/s |     558.3 MB/s |   4.09x |
| citm_catalog.json      | 1.65 MB |     713.4 MB/s |     434.9 MB/s |   1,176.8 MB/s |   1.64x |
| github_events.json     | 63.6 KB |   1,308.7 MB/s |     346.6 MB/s |   1,350.7 MB/s |   3.78x |
| gsoc-2018.json         | 3.17 MB |   1,017.5 MB/s |     306.1 MB/s |   1,959.7 MB/s |   3.32x |
| instruments.json       | 215.2 KB |     986.5 MB/s |     333.6 MB/s |     883.6 MB/s |   2.96x |
| marine_ik.json         | 2.85 MB |     289.8 MB/s |     167.9 MB/s |     398.5 MB/s |   1.73x |
| mesh.json              | 706.6 KB |     435.7 MB/s |     172.9 MB/s |     646.9 MB/s |   2.52x |
| mesh.pretty.json       | 1.50 MB |     614.7 MB/s |     224.9 MB/s |   1,121.2 MB/s |   2.73x |
| numbers.json           | 146.6 KB |     882.1 MB/s |     204.6 MB/s |     902.1 MB/s |   4.31x |
| random.json            | 498.5 KB |     512.7 MB/s |     233.6 MB/s |     546.7 MB/s |   2.20x |
| stringifiedphp.json    | 139.9 KB |   2,383.7 MB/s |     301.9 MB/s |   3,115.3 MB/s |   7.90x |
| twitter.json           | 616.7 KB |   1,030.6 MB/s |     368.1 MB/s |     982.2 MB/s |   2.80x |
| twitterescaped.json    | 549.2 KB |     816.3 MB/s |     288.3 MB/s |     699.7 MB/s |   2.83x |
| update-center.json     | 520.7 KB |     653.8 MB/s |     250.4 MB/s |     692.1 MB/s |   2.61x |

### Encode

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,883.1 MB/s |   1,130.2 MB/s |   1.67x |
| canada.json            | 2.15 MB |     382.8 MB/s |      56.7 MB/s |   6.75x |
| citm_catalog.json      | 1.65 MB |   1,553.1 MB/s |   2,415.8 MB/s |   0.64x |
| github_events.json     | 63.6 KB |   2,160.7 MB/s |   1,288.5 MB/s |   1.68x |
| gsoc-2018.json         | 3.17 MB |     791.5 MB/s |   1,048.4 MB/s |   0.75x |
| instruments.json       | 215.2 KB |   2,533.8 MB/s |   1,915.4 MB/s |   1.32x |
| marine_ik.json         | 2.85 MB |     369.9 MB/s |     124.5 MB/s |   2.97x |
| mesh.json              | 706.6 KB |     497.6 MB/s |      85.8 MB/s |   5.80x |
| mesh.pretty.json       | 1.50 MB |     986.6 MB/s |     189.1 MB/s |   5.22x |
| numbers.json           | 146.6 KB |     615.9 MB/s |      54.9 MB/s |  11.21x |
| random.json            | 498.5 KB |     445.7 MB/s |     526.8 MB/s |   0.85x |
| stringifiedphp.json    | 139.9 KB |   2,622.5 MB/s |   1,682.0 MB/s |   1.56x |
| twitter.json           | 616.7 KB |   1,570.3 MB/s |     999.0 MB/s |   1.57x |
| twitterescaped.json    | 549.2 KB |   1,418.4 MB/s |     932.3 MB/s |   1.52x |
| update-center.json     | 520.7 KB |     956.7 MB/s |     818.6 MB/s |   1.17x |

### Validate

| File                   |     Size |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   2,360.6 MB/s |     363.6 MB/s |   3,852.2 MB/s |   6.49x |
| canada.json            | 2.15 MB |     835.6 MB/s |     102.2 MB/s |   1,273.4 MB/s |   8.17x |
| citm_catalog.json      | 1.65 MB |   2,358.6 MB/s |     532.2 MB/s |   3,529.8 MB/s |   4.43x |
| github_events.json     | 63.6 KB |   2,818.3 MB/s |     380.7 MB/s |   4,263.5 MB/s |   7.40x |
| gsoc-2018.json         | 3.17 MB |   1,403.7 MB/s |     342.5 MB/s |   3,751.0 MB/s |   4.10x |
| instruments.json       | 215.2 KB |   1,969.7 MB/s |     377.3 MB/s |   3,145.2 MB/s |   5.22x |
| marine_ik.json         | 2.85 MB |     821.2 MB/s |     218.7 MB/s |   1,086.7 MB/s |   3.76x |
| mesh.json              | 706.6 KB |   1,142.5 MB/s |     191.0 MB/s |   1,211.5 MB/s |   5.98x |
| mesh.pretty.json       | 1.50 MB |   1,562.0 MB/s |     244.9 MB/s |   2,018.8 MB/s |   6.38x |
| numbers.json           | 146.6 KB |   1,490.0 MB/s |     227.8 MB/s |   1,529.3 MB/s |   6.54x |
| random.json            | 498.5 KB |   1,530.2 MB/s |     305.5 MB/s |   2,461.3 MB/s |   5.01x |
| stringifiedphp.json    | 139.9 KB |   2,519.4 MB/s |     325.7 MB/s |   3,531.1 MB/s |   7.74x |
| twitter.json           | 616.7 KB |   2,487.5 MB/s |     450.5 MB/s |   3,737.8 MB/s |   5.52x |
| twitterescaped.json    | 549.2 KB |   2,271.5 MB/s |     335.8 MB/s |   1,646.1 MB/s |   6.76x |
| update-center.json     | 520.7 KB |   1,991.8 MB/s |     289.7 MB/s |   2,790.3 MB/s |   6.88x |

## Throughput, small corpus

### Decode (objects)

| File                   |    Size |     fastjson |     ext/json |     simdjson | fast/call |  ext/call |   sj/call | speedup |
|------------------------|---------|--------------|--------------|--------------|-----------|-----------|-----------|---------|
| adversarial.json       |   80 B |     327.4 MB/s |     158.9 MB/s |     300.4 MB/s |    233 ns |    480 ns |    254 ns |   2.06x |
| demo.json              |  387 B |     739.6 MB/s |     294.8 MB/s |     654.4 MB/s |    499 ns |   1.3 µs |    564 ns |   2.51x |
| flatadversarial.json   |   64 B |     242.2 MB/s |     125.3 MB/s |     230.3 MB/s |    252 ns |    487 ns |    265 ns |   1.93x |
| repeat.json            | 11.1 KB |     894.9 MB/s |     405.1 MB/s |     973.2 MB/s |  12.1 µs |  26.7 µs |  11.1 µs |   2.21x |
| truenull.json          | 11.7 KB |     809.1 MB/s |     178.9 MB/s |     758.5 MB/s |  14.1 µs |  64.0 µs |  15.1 µs |   4.52x |
| twitter_timeline.json  | 41.2 KB |     825.3 MB/s |     271.2 MB/s |     788.8 MB/s |  48.8 µs | 148.5 µs |  51.1 µs |   3.04x |

### Decode (assoc arrays)

| File                   |    Size |     fastjson |     ext/json |     simdjson | fast/call |  ext/call |   sj/call | speedup |
|------------------------|---------|--------------|--------------|--------------|-----------|-----------|-----------|---------|
| adversarial.json       |   80 B |     365.0 MB/s |     164.8 MB/s |     336.1 MB/s |    209 ns |    463 ns |    227 ns |   2.22x |
| demo.json              |  387 B |     870.5 MB/s |     321.2 MB/s |     788.6 MB/s |    424 ns |   1.1 µs |    468 ns |   2.71x |
| flatadversarial.json   |   64 B |     277.4 MB/s |     134.4 MB/s |     260.8 MB/s |    220 ns |    454 ns |    234 ns |   2.06x |
| repeat.json            | 11.1 KB |   1,076.6 MB/s |     456.1 MB/s |   1,213.3 MB/s |  10.1 µs |  23.7 µs |   8.9 µs |   2.36x |
| truenull.json          | 11.7 KB |     833.4 MB/s |     183.5 MB/s |     764.3 MB/s |  13.7 µs |  62.4 µs |  15.0 µs |   4.54x |
| twitter_timeline.json  | 41.2 KB |     970.8 MB/s |     289.6 MB/s |     879.6 MB/s |  41.5 µs | 139.1 µs |  45.8 µs |   3.35x |

### Encode

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     556.9 MB/s |     615.3 MB/s |    137 ns |    124 ns |   0.91x |
| demo.json              |  387 B |   1,488.2 MB/s |   1,226.2 MB/s |    248 ns |    301 ns |   1.21x |
| flatadversarial.json   |   64 B |     432.9 MB/s |     314.6 MB/s |    141 ns |    194 ns |   1.38x |
| repeat.json            | 11.1 KB |   1,874.7 MB/s |     857.5 MB/s |   5.8 µs |  12.6 µs |   2.19x |
| truenull.json          | 11.7 KB |   1,277.1 MB/s |   1,641.4 MB/s |   9.0 µs |   7.0 µs |   0.78x |
| twitter_timeline.json  | 41.2 KB |   1,642.6 MB/s |   1,140.0 MB/s |  24.5 µs |  35.3 µs |   1.44x |

### Validate

| File                   |    Size |     fastjson |     ext/json |     simdjson | fast/call |  ext/call |   sj/call | speedup |
|------------------------|---------|--------------|--------------|--------------|-----------|-----------|-----------|---------|
| adversarial.json       |   80 B |     641.1 MB/s |     182.5 MB/s |     687.3 MB/s |    119 ns |    418 ns |    111 ns |   3.51x |
| demo.json              |  387 B |   1,557.3 MB/s |     364.7 MB/s |   1,984.3 MB/s |    237 ns |   1.0 µs |    186 ns |   4.27x |
| flatadversarial.json   |   64 B |     530.7 MB/s |     148.5 MB/s |     535.4 MB/s |    115 ns |    411 ns |    114 ns |   3.57x |
| repeat.json            | 11.1 KB |   1,944.7 MB/s |     509.6 MB/s |   3,189.0 MB/s |   5.6 µs |  21.3 µs |   3.4 µs |   3.82x |
| truenull.json          | 11.7 KB |   2,375.8 MB/s |     204.2 MB/s |   2,252.8 MB/s |   4.8 µs |  56.1 µs |   5.1 µs |  11.64x |
| twitter_timeline.json  | 41.2 KB |   2,609.4 MB/s |     307.2 MB/s |   2,826.2 MB/s |  15.4 µs | 131.1 µs |  14.3 µs |   8.49x |

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
| apache_builds.json     | 124.3 KB |     405.8 KB |     100.0 KB |     4.06x |
| canada.json            | 2.15 MB |     10.87 MB |      2.00 MB |     5.45x |
| citm_catalog.json      | 1.65 MB |      3.08 MB |     492.0 KB |     6.41x |
| github_events.json     | 63.6 KB |     217.8 KB |      56.0 KB |     3.89x |
| gsoc-2018.json         | 3.17 MB |      7.63 MB |      2.94 MB |     2.59x |
| instruments.json       | 215.2 KB |     733.8 KB |     108.0 KB |     6.79x |
| marine_ik.json         | 2.85 MB |     19.27 MB |      1.75 MB |    11.03x |
| mesh.json              | 706.6 KB |      4.93 MB |     628.0 KB |     8.04x |
| mesh.pretty.json       | 1.50 MB |      4.93 MB |     628.0 KB |     8.04x |
| numbers.json           | 146.6 KB |     709.8 KB |     148.0 KB |     4.80x |
| random.json            | 498.5 KB |      2.90 MB |     656.0 KB |     4.53x |
| adversarial.json       |   80 B |        848 B |        336 B |     2.52x |
| demo.json              |  387 B |       2.1 KB |        256 B |     8.50x |
| flatadversarial.json   |   64 B |        864 B |        352 B |     2.45x |
| repeat.json            | 11.1 KB |      53.8 KB |      12.0 KB |     4.48x |
| truenull.json          | 11.7 KB |      97.8 KB |      12.0 KB |     8.15x |
| twitter_timeline.json  | 41.2 KB |     189.8 KB |      44.0 KB |     4.31x |
| stringifiedphp.json    | 139.9 KB |     948.5 KB |     140.0 KB |     6.78x |
| twitter.json           | 616.7 KB |      2.00 MB |     556.0 KB |     3.68x |
| twitterescaped.json    | 549.2 KB |      2.00 MB |     556.0 KB |     3.68x |
| update-center.json     | 520.7 KB |      1.81 MB |     532.0 KB |     3.49x |

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
| Decode (objects)       | 14.81 MB |     530.5 MB/s |     196.1 MB/s |     714.9 MB/s |   2.70x |
| Decode (assoc arrays)  | 14.81 MB |     519.9 MB/s |     198.0 MB/s |     740.1 MB/s |   2.63x |
| Encode                 | 14.81 MB |     614.0 MB/s |     172.0 MB/s |             - |   3.57x |
| Validate               | 14.81 MB |   1,240.5 MB/s |     229.6 MB/s |   1,841.7 MB/s |   5.40x |

### Throughput, small corpus

| Operation              |    Bytes |     fastjson |     ext/json |     simdjson | speedup |
|------------------------|----------|--------------|--------------|--------------|---------|
| Decode (objects)       | 64.6 KB |     829.3 MB/s |     261.2 MB/s |     804.7 MB/s |   3.18x |
| Decode (assoc arrays)  | 64.6 KB |     953.5 MB/s |     277.5 MB/s |     893.0 MB/s |   3.44x |
| Encode                 | 64.6 KB |   1,585.0 MB/s |   1,135.2 MB/s |             - |   1.40x |
| Validate               | 64.6 KB |   2,398.3 MB/s |     299.9 MB/s |   2,725.3 MB/s |   8.00x |

### Memory peak (across all files)

| Operation              |     fastjson |     ext/json |     simdjson |  fast/ext |
|------------------------|--------------|--------------|--------------|-----------|
| Decode (objects)       |     97.81 MB |     56.37 MB |     56.36 MB |     1.74x |
| Decode (assoc arrays)  |     96.38 MB |     54.94 MB |     54.93 MB |     1.75x |
| Encode                 |     62.70 MB |     11.24 MB |            - |     5.58x |
| Validate               |     14.91 MB |     150.6 KB |          0 B |   101.40x |

_Throughput: higher is better; speedup = ext/json time / fastjson time. Memory: lower is better; ratio = fastjson peak / ext/json peak (>1 means fastjson uses more)._
