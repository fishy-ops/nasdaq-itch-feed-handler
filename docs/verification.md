# Verification and handoff — September 24, 2026

The active implementation is commit `3c4ad2ec2592c08b0c4446f068b9988ab1f7e7ea`.
Subsequent handoff commits add measured results and documentation only.

| Requirement | Evidence |
|---|---|
| Private fishy-ops repository | `gh repo view` confirmed PRIVATE for [fishy-ops/nasdaq-itch-feed-handler](https://github.com/fishy-ops/nasdaq-itch-feed-handler) |
| Comparable external implementation | Unmodified bbalouki/itchcpp at `8af52e5ebcc1a112749a28510d998f56a031db8f`; isolated public-API adapters, matching compiler/flags |
| Correctness before comparison | All three profiles matched independently generated message counts, decoded-field digests, and every final price/quantity/order-count row for original, optimized, and peer engines |
| Reproducible baseline | [Initial report and raw samples](comparison-baseline/README.md), including input and executable SHA-256 hashes |
| Measured improvement | [Profiling case study and nine-round paired samples](optimization-level-links/README.md); original binary hash verified before use |
| Local tests | 21/21 Release and 21/21 UBSan cases passed, including link lifetime/copy/move tests and independent workload generation |
| Linux and macOS builds/tests | All four Debug/Release platform jobs passed in [run 36040733357](https://github.com/fishy-ops/nasdaq-itch-feed-handler/actions/runs/36040733357) |
| Linux address/undefined sanitizers | Sanitizer job passed in the same run for implementation commit `3c4ad2e` |
| External adapters across platforms | [Run 36040733278](https://github.com/fishy-ops/nasdaq-itch-feed-handler/actions/runs/36040733278) passed the three profile checks on Linux and macOS |

The original slower results are preserved. Balanced/wide improvements are supported
within the documented runs; the variable deep-book run is not presented as a
reliable peer speedup. No real exchange day, production system, or network latency
was benchmarked. Public Nasdaq sample listings were inspected, but no historical
market dataset was downloaded or redistributed.

The user's work cutoff was 10 AM America/Chicago. A clock check returned
18:22 UTC (1:22 PM Central), showing the cutoff had already passed. No additional
optimization was started after that check; only recording the already-completed
measurements, checking existing CI, and publishing this handoff followed.
