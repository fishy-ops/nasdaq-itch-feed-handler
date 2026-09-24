# Reducing repeated lookups without weakening validation

## Evidence and change

A three-second macOS `sample` trace of repeated baseline balanced-book replays
showed substantial stacks in `OrderBook::reduce`, `OrderBook::insert`, and
`InstrumentRegistry::at`. The [raw trace](profile-baseline.txt) and the exact
[profiling driver](profile-driver.cpp) are retained. The driver repeats the same
book path 200 times to keep a process alive long enough for sampling. Profiling
measurements themselves are not throughput results.

Previously, a reduction located the order during validation, looked it up again
to update it, looked up the instrument book, and searched the price tree. Now the
validated order iterator is reused and each stored order holds a link to its
stable `std::map` level. Partial reductions update that level directly. A price
tree lookup is needed only when the last order leaves a level. Insertions and
replacements still use the existing ordered price maps.

Directory lookups now use a locate-indexed vector of pointers to owned map
entries. The ordered map remains the owner and keeps output deterministic.
Missing and sparse locates still fail explicitly. The cache is bounded by the
16-bit stock-locate range: at most 65,536 pointer slots.

Both changes preserve duplicate-reference, missing-reference, quantity, symbol,
and locate validation. No allocation pool, relaxed error mode, custom hash table,
or external optimization library was introduced. A stored order becomes larger
because it carries a level pointer; the registry uses extra pointer-array memory.
Those are explicit speed/memory tradeoffs, not a claim of free improvement.

## Lifetime safety

Map nodes remain stable across insertions. A level is removed only when its last
order disappears, so a live order cannot point to a removed level. Rehashing the
order map does not move the price-level nodes. Custom copy operations rebuild
links into the new owners; default move operations transfer the owners and links.
The tests force order-table rehashes, copy and move populated books, remove/recreate
levels, replace the only order at the same price, and access sparse/max locates.
The existing generated reference and independent Python workload oracles remain
in place.

## Reproduce profiling

Build baseline commit `ac3f652` as described in the comparison instructions, then
compile `profile-driver.cpp` with C++20 and `-O3 -DNDEBUG`, include paths pointing
to that checkout's `include` and `benchmarks/comparison`, and link its
`libitch_core.a`. Run the driver with the generated balanced input path. On macOS,
`sample PID 3 -file profile.txt` samples that live process for three seconds. Stop
the driver after collecting the sample. Sampling perturbs execution; do not mix
these samples with throughput measurements.

## Measuring the change

The runner accepts `--initial-executable` and `--initial-result` together to add
an original executable to the comparison. It verifies that the executable's
SHA-256 matches the original recorded result and that both results refer to the
same input SHA-256. All three engines must pass the independent oracle. Nine
rounds rotate the launch order equally among original, updated, and peer engines.
Run this only while the machine is otherwise idle; no builds or profiling should
run concurrently.

On another host, first build the original commit and create a local baseline
result, retain that executable, then build the optimized commit. Binary hashes
are machine/build-specific: use your own original result, not a downloaded
executable or this host's expected binary hash.

## Recorded three-way results

Updated commit `3c4ad2e` was measured against the preserved original binary from `ac3f652` and the same pinned peer. All three passed the independent decoded-field and exact final-level checks for each workload. Raw nine-round samples are linked below; metadata records clean source status and executable/input SHA-256 values.

| Profile | Updated M msg/s | Original M msg/s | Peer M msg/s | Paired updated/original median (range) | Paired updated/peer median (range) |
|---|---:|---:|---:|---:|---:|
| [balanced](balanced.json) | 15.36 | 9.43 | 20.76 | 1.58× (1.49–1.69) | 0.73× (0.69–0.84) |
| [deep](deep.json) | 11.32 | 5.38 | 9.96 | 1.99× (1.37–2.54) | 1.16× (0.64–1.61) |
| [wide](wide.json) | 11.67 | 7.29 | 15.82 | 1.59× (1.57–1.68) | 0.73× (0.68–0.79) |

Balanced and wide profiles show repeatable improvements over the original binary within these nine pairs: approximately **58% and 59% higher median paired throughput**. The updated pipeline is still slower than the peer on those profiles (about 73% of peer throughput). These are results for the documented synthetic workload, not a general speed guarantee.

The deep profile had substantial run-to-run variation (updated throughput ranged from 5.83 to 14.37 M messages/s). Its paired updated/peer ratio ranges from 0.64× to 1.61× and crosses 1. The observed median favors the updated implementation, but **this run does not support a reliable claim of outperforming the peer**. No rerun was selected to make that result look stronger.

The timed boundary and semantic differences remain those documented in the [comparison methodology](../../benchmarks/comparison/README.md). Process-memory high-water marks include the input and warmup, so small RSS changes should not be described as book-memory savings.

## Resume and interview wording supported by this evidence

> Built a C++20 Nasdaq ITCH replay engine and validated its decoded fields and full final order-book depth against an independent model and a pinned open-source C++ peer. Used CPU profiling to remove repeated lookups, increasing median paired pipeline throughput by 58–59% over the original implementation on two documented multi-symbol synthetic workloads.

If asked how it compares with another implementation:

> On the same Apple M4 Pro and inputs, my optimized pipeline reached about 73% of the peer’s throughput on the balanced and wide workloads. It preserves strict order-reference, quantity, and directory checks. The peer maintains additional FIFO/BBO state. The comparison helped me identify and measure an optimization without claiming the two systems have identical features.

Local Release and UBSan suites passed 21/21 cases after the change. The exact GitHub workflow results are linked in the final verification record.
