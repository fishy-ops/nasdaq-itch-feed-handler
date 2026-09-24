# Same-host baseline comparison

This harness compares `fishy-ops/nasdaq-itch-feed-handler` with the public
[`bbalouki/itchcpp`](https://github.com/bbalouki/itchcpp/tree/8af52e5ebcc1a112749a28510d998f56a031db8f)
parser and `BookManager`, pinned to commit `8af52e5ebcc1a112749a28510d998f56a031db8f`.
The peer is MIT licensed. It is fetched separately; no peer source or binaries are
redistributed in this repository. Its full license remains in that checkout.

Both implementations are compiled with the same compiler, C++20 standard, and
Release flags. Four unmodified peer implementation units (`parser`, `messages`,
`l3_book`, `book_manager`) are linked to an adapter using the public parser callback
and full-market BookManager interfaces. Two executables prevent same-namespace
collisions. No peer parser or book source is patched, simplified, or replaced.

## Reproduce

From the project root, using Python 3.11+ for the comparison runner:

```sh
git clone https://github.com/bbalouki/itchcpp.git ../itchcpp-baseline
git -C ../itchcpp-baseline checkout 8af52e5ebcc1a112749a28510d998f56a031db8f
cmake -S . -B build-compare -DCMAKE_BUILD_TYPE=Release \
  -DITCH_COMPARISON_PEER="$(cd ../itchcpp-baseline && pwd)"
cmake --build build-compare --parallel
ctest --test-dir build-compare --output-on-failure
python3 tools/generate_workload.py balanced.itch --profile balanced --updates 1000000
python3 benchmarks/comparison/run.py --build build-compare --input balanced.itch \
  --output balanced-comparison.json --rounds 7 --hardware 'YOUR CPU, RAM, OS'
```

Repeat for `deep` and `wide`. Input and the independent `.expected.json` manifest
must remain together. CMake rejects a different peer commit; the runner also
rejects a modified peer checkout, a changed input, a non-Release timed build, or
any correctness mismatch. Normal project builds do not fetch this dependency.
Use `--verify-only` for correctness checks without collecting timing.

## Workloads fixed before measurement

| Profile | Symbols | Concurrent orders | Candidate price levels per side/symbol |
|---|---:|---:|---:|
| balanced | 32 | 8,192 | 64 |
| deep | 1 | 32,768 | 4,096 |
| wide | 256 | 16,384 | 32 |
| tiny (tests) | 3 | 48 | 8 |

Each begins with directories and trading states, then fills alternating bids and
asks. Updates select a uniformly random live order. The mix is 25% replacements,
20% delete/re-add pairs, 20% cancellations, 25% executions, and 10% executions with
price; depleted orders are replenished. One fifth of additions use attribution.
Quantities range from 100 to 1000 on add/replace. Actual encoded counts differ from
update count because deletion/depletion can emit a second message. Seeds, counts,
input SHA-256, decoded-field digest, and full expected final levels are recorded
by the independent Python generator. These are synthetic stress profiles, not an
empirical model of exchange traffic or proof of market-wide speed.

## Correctness gate

Before timings, each executable runs an untimed validation pass. Both must match
the independent generator's message count, digest of every field retained by the
self implementation, and **all final price levels, quantities, and order counts**.
The digest is a noncryptographic corruption check, not a formal proof. Final
states are compared exactly, not only their top of book or an empty final state.
The normal test suite also checks independently recomputed levels after each of
5,000 generated book events, and the Python workload oracle checks three seeds.
This harness does not claim that intermediate external-peer states are equal at
every message; that would require an additional checkpoint comparison.

## Timed boundary and statistics

Two paths are measured: framing plus eager decoding (`decode`) and framing,
decoding, and public book dispatch (`book`). Input loading, validation digests,
final depth export, and JSON output are outside the timed region. The measured
function includes construction/destruction, a message counter, and a header sum
used to keep work observable. Each child process first runs one untimed warmup,
then one timed pass with a fresh engine. No LTO is configured. Both libraries are
compiled as separate implementation units; a header-only framing walk is not
substituted for actual decoding.

The runner alternates which engine goes first in each pair, runs sequentially,
and records every raw sample. Report median throughput, per-engine min/max, and
the median of paired throughput ratios. A ratio above 1 favors fishy-ops on this
specific path/input; below 1 favors the peer. Small differences within the
observed spread should not be promoted as a dependable advantage. Seven pairs
are descriptive measurements, not a statistical significance test.

Peak RSS is measured per child process and includes the input buffer, warmup
high-water mark, runtime, and book. It is process memory, not isolated per-order
storage. No inter-process memory subtraction or comparison with published
benchmarks on different hardware is performed. OS scheduling, clock scaling,
cache state, and other host activity remain uncontrolled and must be disclosed.

## Semantic differences that constrain claims

The peer retains FIFO order priority, tracks BBO changes internally, decodes more
administrative fields/message types, and offers optional trade/BBO callbacks.
Those callbacks are left unset, but its default internal work remains intact.
Fishy-ops retains aggregate depth and strict reference/quantity checks, including
directory consistency. Thus book timings compare actual supported pipelines;
they do not isolate a single data structure or imply equivalent overall features.
Only the ten shared supported message types are generated. No unsupported-message
skipping advantage is included. Error-handling differences are not timed.

Keep results favorable to either implementation. A legitimate portfolio statement
must name the peer/version, exact workload, hardware, timed boundary, and tradeoff.
Do not claim that this project beats banks' systems, production feeds, or ITCH
implementations generally.
