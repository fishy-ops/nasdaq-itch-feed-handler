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
