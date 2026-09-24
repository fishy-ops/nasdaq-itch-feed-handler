# Initial external comparison — September 24, 2026

This is the first same-host comparison, before any comparison-driven optimization. Fishy-ops revision `ac3f652` is compared with unmodified [`bbalouki/itchcpp` revision `8af52e5`](https://github.com/bbalouki/itchcpp/tree/8af52e5ebcc1a112749a28510d998f56a031db8f). The peer was faster on every measured path/profile. These results are retained as the baseline, not hidden or replaced by later improvements.

Host: Apple M4 Pro, 48 GiB RAM, macOS 27.0; Apple Clang 17; C++20; `-O3 -DNDEBUG`; no LTO, CPU pinning, or host isolation. Each profile uses one million requested updates plus initialization/replenishment events. Seven alternating pairs per mode; each process performs one warmup and one timed replay.

| Profile | Encoded messages | Active orders | Symbols |
|---|---:|---:|---:|
| balanced | 1,226,033 | 8,192 | 32 |
| deep | 1,249,433 | 32,768 | 1 |
| wide | 1,234,500 | 16,384 | 256 |

| Profile / timed path | Fishy-ops median M msg/s | Peer median M msg/s | Median paired self/peer | Paired min–max |
|---|---:|---:|---:|---:|
| balanced / decode | 96.58 | 139.29 | 0.692× | 0.599–0.743× |
| balanced / book | 9.79 | 20.62 | 0.471× | 0.463–0.516× |
| deep / decode | 94.96 | 135.71 | 0.692× | 0.678–0.753× |
| deep / book | 8.02 | 12.17 | 0.672× | 0.617–0.813× |
| wide / decode | 94.87 | 135.70 | 0.706× | 0.640–0.717× |
| wide / book | 7.73 | 16.52 | 0.468× | 0.455–0.480× |

The ratio is the median of paired ratios, not a ratio inferred from independently rounded table entries. `decode` includes framing and eager decoding. `book` additionally includes each library’s public dispatch and book maintenance. These measurements include engine construction/destruction but exclude input loading, full validation, depth export, and JSON output.

## Validation and scope

Both engines matched the independent Python oracle’s message count, decoded-field digest, and exact final levels (all prices, quantities, and order counts) on every input. The final books are nonempty. The input SHA-256, expected-manifest SHA-256, compiler output, build flags, binary hashes, and every raw sample are stored in the three JSON files in this directory.

The peer also maintains FIFO order priority and BBO change state, and it has broader decoding capabilities. Fishy-ops performs strict invalid-reference, quantity, and directory checks. These differences prevent interpreting the result as an isolated data-structure race or as identical overall feature sets. No live network feed, exchange-to-client latency, full historical day, or proprietary banking system was measured. The `deep` profile is a price-depth stress case; all three are synthetic. Per-process peak RSS in the JSON includes the input and warmup high-water mark; it is not isolated order-book memory.

## Defensible interview wording

> I built a C++20 Nasdaq ITCH replay engine and a reproducible comparison against a pinned open-source C++ implementation. Both matched an independent final-book oracle across three synthetic workloads with 8,192–32,768 active orders. My initial implementation processed about 7.7–9.8 million messages per second through its in-memory framing, decode, and book pipeline on an Apple M4 Pro, which was 47–67% of the peer’s throughput. I used that measured gap to guide subsequent work.

The final sentence should only be used once subsequent work has actually occurred. Do not claim a speedup over the peer from these results. The older one-active-order benchmark is a different workload and is not a direct comparator.

## Reproduction

Use [the harness instructions](../../benchmarks/comparison/README.md) at revision `ac3f652` to reproduce this baseline. Generate each profile with `--updates 1000000 --seed 20260924` and run seven pairs. Paths in raw metadata identify the original local checkout and are not required to exist on a new machine. No market-data files or third-party binaries are included.
