# Performance and verification

## Measured baseline — September 24, 2026

Environment: Apple M4 Pro, 48 GiB RAM, macOS 27.0 (26A428), arm64, Apple Clang 17.0.0.17000604, CMake Release (`-O3 -DNDEBUG`, C++20). No CPU affinity, frequency control, or exclusive host isolation was applied.

Dataset: generated with `generate_fixture benchmark.itch 250000`: 1,000,004 messages, 32,250,096 bytes, payload FNV-1a-64 `eebd33a5b85a13a6`. The fingerprint covers concatenated payload bytes and is an identity aid, not a cryptographic integrity check. Each cycle adds, partially executes, replaces, then deletes an order. At most one order is live. This is a deliberately small-state throughput workload, **not representative of a full exchange session or a large order book**.

| Measurement | Result |
|---|---:|
| Decode-only median, 5 runs | 72.32 million messages/s |
| Decode + registry + book + counters median, 5 runs | 23.58 million messages/s |
| Streaming replay, one run | 16.00 million messages/s |
| Streaming replay elapsed | 0.06250 s |
| Benchmark process peak RSS | 102.77 MiB |

Raw measurements: [benchmark-results.txt](benchmark-results.txt) and [replay-benchmark.json](replay-benchmark.json). The memory measurement includes preloaded frames, vector capacity, warmup, runtime, and books; it is not isolated book storage. Short runs and a tiny active book make these numbers sensitive to scheduling and cache behavior. Do not extrapolate these results to large datasets, live feed capacity, or latency guarantees.

Decode-only timing excludes file loading and framing. Decode-and-book timing includes the replay dispatcher, directory lookup, event counters, and book updates, but excludes file loading, framing, snapshots, final output, and object destruction. A validation/warmup pass runs before timing. Each iteration has a fresh book; a printed checksum makes decoded work observable. The streaming report times the reading/decoding/application loop, excluding file open, final quote output, and JSON writing; OS cache state is uncontrolled.

## Reproduce

Run from the project root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/generate_fixture benchmark.itch 250000
./build/itch_benchmark benchmark.itch 5
./build/itch_replay --input benchmark.itch --report replay-benchmark.json
```

The raw report's `work/benchmark.itch` path identifies the original local run; regenerated input has the same bytes regardless of path. Larger cycle counts extend run duration, but still do not increase simultaneous book depth. A realistic next benchmark should replay an authorized full-day sample or add a synthetic workload with many concurrently active orders and a wider symbol distribution.

No optimization case study is claimed: this is a measured standard-container baseline, and no profiling-supported optimization was established. A future change should record the profile, compare repeated runs on the same workload, verify the book against the reference model, and describe memory/performance tradeoffs.

## Verification status

- Local macOS Release build: passed without project compiler warnings.
- Local test suite: 17/17 CTest cases passed, including the 5,000-event reference comparison and CLI subprocess checks.
- Synthetic demo: exact expected AAPL/MSFT books; JSON and CSV outputs inspected.
- Linux/macOS Debug and Release jobs, plus Linux ASan/UBSan: passed for initial commit `626ac82` in [GitHub Actions run 35976578446](https://github.com/fishy-ops/nasdaq-itch-feed-handler/actions/runs/35976578446).
- Local macOS Debug build: 17/17 tests passed.
- Local macOS UndefinedBehaviorSanitizer-only Debug build: 17/17 tests passed with `UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1`.
- Local AddressSanitizer: blocked by runtime startup on this host. Both the project executable and a trivial standalone `puts` program stalled before `main`, including outside the sandbox with leak detection disabled. This is not reported as a passing ASan check; the Linux CI sanitizer job remains the intended verification path.

## Same-host external comparison

The initial one-order microbenchmark above is retained as a historical baseline.
The [comparison harness](../benchmarks/comparison/README.md) adds larger active books,
independent correctness gates, and an unmodified pinned external C++ implementation.
Only measurements recorded by that harness support claims about the peer.
