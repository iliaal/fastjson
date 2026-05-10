# fastjson benchmark

- PHP 8.4.22-dev
- fastjson 0.1.0 (yyjson 0.12.0)
- ext/json 8.4.22-dev
- 200 iterations per case (slowest 10% dropped)
- CPU: 13th Gen Intel(R) Core(TM) i9-13950HX

## Throughput, large corpus

### Decode (objects)

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,042.2 MB/s |     396.9 MB/s |   2.63x |
| canada.json            | 2.15 MB |     467.6 MB/s |     102.4 MB/s |   4.57x |
| citm_catalog.json      | 1.65 MB |   1,021.9 MB/s |     490.2 MB/s |   2.08x |
| github_events.json     | 63.6 KB |   1,379.1 MB/s |     452.7 MB/s |   3.05x |
| gsoc-2018.json         | 3.17 MB |   1,150.2 MB/s |     340.1 MB/s |   3.38x |
| instruments.json       | 215.2 KB |     850.0 MB/s |     342.6 MB/s |   2.48x |
| marine_ik.json         | 2.85 MB |     323.3 MB/s |     200.1 MB/s |   1.62x |
| mesh.json              | 706.6 KB |     521.3 MB/s |     210.4 MB/s |   2.48x |
| mesh.pretty.json       | 1.50 MB |     743.2 MB/s |     270.7 MB/s |   2.75x |
| numbers.json           | 146.6 KB |   1,039.6 MB/s |     245.5 MB/s |   4.23x |
| random.json            | 498.5 KB |     520.4 MB/s |     270.7 MB/s |   1.92x |
| stringifiedphp.json    | 139.9 KB |   2,697.4 MB/s |     364.4 MB/s |   7.40x |
| twitter.json           | 616.7 KB |   1,082.4 MB/s |     425.5 MB/s |   2.54x |
| twitterescaped.json    | 549.2 KB |     880.5 MB/s |     325.0 MB/s |   2.71x |
| update-center.json     | 520.7 KB |     644.7 MB/s |     291.3 MB/s |   2.21x |

### Decode (assoc arrays)

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,118.3 MB/s |     427.1 MB/s |   2.62x |
| canada.json            | 2.15 MB |     449.8 MB/s |     105.0 MB/s |   4.28x |
| citm_catalog.json      | 1.65 MB |   1,216.2 MB/s |     519.0 MB/s |   2.34x |
| github_events.json     | 63.6 KB |   1,436.2 MB/s |     449.9 MB/s |   3.19x |
| gsoc-2018.json         | 3.17 MB |   1,135.3 MB/s |     370.7 MB/s |   3.06x |
| instruments.json       | 215.2 KB |     895.1 MB/s |     394.9 MB/s |   2.27x |
| marine_ik.json         | 2.85 MB |     340.9 MB/s |     211.1 MB/s |   1.61x |
| mesh.json              | 706.6 KB |     511.6 MB/s |     208.8 MB/s |   2.45x |
| mesh.pretty.json       | 1.50 MB |     832.9 MB/s |     270.7 MB/s |   3.08x |
| numbers.json           | 146.6 KB |   1,015.7 MB/s |     259.3 MB/s |   3.92x |
| random.json            | 498.5 KB |     596.8 MB/s |     300.1 MB/s |   1.99x |
| stringifiedphp.json    | 139.9 KB |   2,790.9 MB/s |     376.2 MB/s |   7.42x |
| twitter.json           | 616.7 KB |   1,184.1 MB/s |     451.0 MB/s |   2.63x |
| twitterescaped.json    | 549.2 KB |     978.3 MB/s |     350.7 MB/s |   2.79x |
| update-center.json     | 520.7 KB |     719.4 MB/s |     305.9 MB/s |   2.35x |

### Encode

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   1,681.2 MB/s |   1,260.0 MB/s |   1.33x |
| canada.json            | 2.15 MB |     801.2 MB/s |      60.3 MB/s |  13.28x |
| citm_catalog.json      | 1.65 MB |   3,156.2 MB/s |   2,483.2 MB/s |   1.27x |
| github_events.json     | 63.6 KB |   1,997.4 MB/s |   1,297.9 MB/s |   1.54x |
| gsoc-2018.json         | 3.17 MB |   1,393.8 MB/s |     778.7 MB/s |   1.79x |
| instruments.json       | 215.2 KB |   2,237.8 MB/s |   1,810.1 MB/s |   1.24x |
| marine_ik.json         | 2.85 MB |     698.2 MB/s |     136.3 MB/s |   5.12x |
| mesh.json              | 706.6 KB |     766.7 MB/s |      89.2 MB/s |   8.60x |
| mesh.pretty.json       | 1.50 MB |   1,668.3 MB/s |     191.8 MB/s |   8.70x |
| numbers.json           | 146.6 KB |     575.0 MB/s |      54.7 MB/s |  10.50x |
| random.json            | 498.5 KB |     929.0 MB/s |     651.4 MB/s |   1.43x |
| stringifiedphp.json    | 139.9 KB |   3,099.3 MB/s |     790.3 MB/s |   3.92x |
| twitter.json           | 616.7 KB |   1,658.2 MB/s |   1,113.7 MB/s |   1.49x |
| twitterescaped.json    | 549.2 KB |   1,502.5 MB/s |   1,011.6 MB/s |   1.49x |
| update-center.json     | 520.7 KB |   1,071.0 MB/s |     843.8 MB/s |   1.27x |

### Validate

| File                   |     Size |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| apache_builds.json     | 124.3 KB |   2,643.6 MB/s |     451.9 MB/s |   5.85x |
| canada.json            | 2.15 MB |     917.7 MB/s |     115.2 MB/s |   7.97x |
| citm_catalog.json      | 1.65 MB |   2,663.2 MB/s |     561.7 MB/s |   4.74x |
| github_events.json     | 63.6 KB |   3,122.1 MB/s |     497.2 MB/s |   6.28x |
| gsoc-2018.json         | 3.17 MB |   1,589.6 MB/s |     388.0 MB/s |   4.10x |
| instruments.json       | 215.2 KB |   2,174.3 MB/s |     445.1 MB/s |   4.88x |
| marine_ik.json         | 2.85 MB |     863.7 MB/s |     266.6 MB/s |   3.24x |
| mesh.json              | 706.6 KB |   1,257.4 MB/s |     223.5 MB/s |   5.63x |
| mesh.pretty.json       | 1.50 MB |   1,673.3 MB/s |     277.7 MB/s |   6.03x |
| numbers.json           | 146.6 KB |   1,501.1 MB/s |     265.6 MB/s |   5.65x |
| random.json            | 498.5 KB |   1,582.9 MB/s |     365.1 MB/s |   4.34x |
| stringifiedphp.json    | 139.9 KB |   2,939.4 MB/s |     366.9 MB/s |   8.01x |
| twitter.json           | 616.7 KB |   2,801.5 MB/s |     560.4 MB/s |   5.00x |
| twitterescaped.json    | 549.2 KB |   2,539.9 MB/s |     403.7 MB/s |   6.29x |
| update-center.json     | 520.7 KB |   2,288.8 MB/s |     358.5 MB/s |   6.38x |

## Throughput, small corpus

### Decode (objects)

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     345.2 MB/s |     187.0 MB/s |    221 ns |    408 ns |   1.85x |
| demo.json              |  387 B |     820.2 MB/s |     335.5 MB/s |    450 ns |   1.1 µs |   2.44x |
| flatadversarial.json   |   64 B |     285.2 MB/s |     144.3 MB/s |    214 ns |    423 ns |   1.98x |
| repeat.json            | 11.1 KB |     979.3 MB/s |     472.8 MB/s |  11.1 µs |  22.9 µs |   2.07x |
| truenull.json          | 11.7 KB |     903.2 MB/s |     213.7 MB/s |  12.7 µs |  53.6 µs |   4.23x |
| twitter_timeline.json  | 41.2 KB |     955.6 MB/s |     341.0 MB/s |  42.1 µs | 118.1 µs |   2.80x |

### Decode (assoc arrays)

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     381.5 MB/s |     197.1 MB/s |    200 ns |    387 ns |   1.94x |
| demo.json              |  387 B |     932.0 MB/s |     354.2 MB/s |    396 ns |   1.0 µs |   2.63x |
| flatadversarial.json   |   64 B |     319.6 MB/s |     153.7 MB/s |    191 ns |    397 ns |   2.08x |
| repeat.json            | 11.1 KB |   1,143.7 MB/s |     516.8 MB/s |   9.5 µs |  21.0 µs |   2.21x |
| truenull.json          | 11.7 KB |     903.9 MB/s |     214.1 MB/s |  12.7 µs |  53.5 µs |   4.22x |
| twitter_timeline.json  | 41.2 KB |   1,064.0 MB/s |     344.1 MB/s |  37.9 µs | 117.0 µs |   3.09x |

### Encode

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     641.1 MB/s |     641.1 MB/s |    119 ns |    119 ns |   1.00x |
| demo.json              |  387 B |   1,647.6 MB/s |   1,351.9 MB/s |    224 ns |    273 ns |   1.22x |
| flatadversarial.json   |   64 B |     473.1 MB/s |     321.2 MB/s |    129 ns |    190 ns |   1.47x |
| repeat.json            | 11.1 KB |   1,762.7 MB/s |   1,030.1 MB/s |   6.1 µs |  10.5 µs |   1.71x |
| truenull.json          | 11.7 KB |   2,323.7 MB/s |   1,561.9 MB/s |   4.9 µs |   7.3 µs |   1.49x |
| twitter_timeline.json  | 41.2 KB |   1,665.8 MB/s |   1,183.4 MB/s |  24.2 µs |  34.0 µs |   1.41x |

### Validate

| File                   |    Size |     fastjson |     ext/json | fast/call |  ext/call | speedup |
|------------------------|---------|--------------|--------------|-----------|-----------|---------|
| adversarial.json       |   80 B |     699.9 MB/s |     215.5 MB/s |    109 ns |    354 ns |   3.25x |
| demo.json              |  387 B |   1,864.0 MB/s |     395.2 MB/s |    198 ns |    934 ns |   4.72x |
| flatadversarial.json   |   64 B |     586.9 MB/s |     174.9 MB/s |    104 ns |    349 ns |   3.36x |
| repeat.json            | 11.1 KB |   2,131.9 MB/s |     583.8 MB/s |   5.1 µs |  18.6 µs |   3.65x |
| truenull.json          | 11.7 KB |   2,466.9 MB/s |     256.9 MB/s |   4.6 µs |  44.5 µs |   9.60x |
| twitter_timeline.json  | 41.2 KB |   2,909.1 MB/s |     413.4 MB/s |  13.8 µs |  97.4 µs |   7.04x |

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
| Decode (objects)       | 14.81 MB |     602.3 MB/s |     226.8 MB/s |   2.66x |
| Decode (assoc arrays)  | 14.81 MB |     628.3 MB/s |     236.9 MB/s |   2.65x |
| Encode                 | 14.81 MB |   1,092.2 MB/s |     180.1 MB/s |   6.06x |
| Validate               | 14.81 MB |   1,352.1 MB/s |     265.2 MB/s |   5.10x |

### Throughput, small corpus

| Operation              |    Bytes |     fastjson |     ext/json | speedup |
|------------------------|----------|--------------|--------------|---------|
| Decode (objects)       | 64.6 KB |     944.5 MB/s |     320.9 MB/s |   2.94x |
| Decode (assoc arrays)  | 64.6 KB |   1,037.6 MB/s |     326.3 MB/s |   3.18x |
| Encode                 | 64.6 KB |   1,765.3 MB/s |   1,202.0 MB/s |   1.47x |
| Validate               | 64.6 KB |   2,630.1 MB/s |     388.9 MB/s |   6.76x |

### Memory peak (across all files)

| Operation              |     fastjson |     ext/json |  fast/ext |
|------------------------|--------------|--------------|-----------|
| Decode (objects)       |     97.81 MB |     56.37 MB |     1.74x |
| Decode (assoc arrays)  |     96.38 MB |     54.94 MB |     1.75x |
| Encode                 |     11.92 MB |     11.24 MB |     1.06x |
| Validate               |     14.91 MB |     150.6 KB |   101.40x |

_Throughput: higher is better; speedup = ext/json time / fastjson time. Memory: lower is better; ratio = fastjson peak / ext/json peak (>1 means fastjson uses more)._
