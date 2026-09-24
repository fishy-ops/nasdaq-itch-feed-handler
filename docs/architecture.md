# Architecture and design decisions

## Data flow and public interfaces

`Reader::next(Frame&)` consumes a binary stream and returns false only at clean EOF. Offsets identify the first byte of the two-byte length prefix. A frame owns the payload, and the next read reuses its buffer. `decode(span, offset)` returns an `Event` containing a common header and a `std::variant` payload. Unsupported messages have an explicit variant alternative. Structured `Error` exceptions retain offset and type.

`Replay::process(event, offset)` updates the instrument registry and dispatches retained order events. `Replay::run(stream, snapshots)` adds framing, decoding, timing, error accounting, and interval snapshots. Create a new Replay instance for each input session; concurrent calls and reuse across sessions are unsupported. `OrderBook::apply(event)` can also be used directly; its caller supplies directory/symbol validation. `depth(locate, side, count)` returns best-first levels; requesting one level retrieves the best bid or ask.

`Metrics::write_json` writes processing status, errors, message counts by type, bytes consumed, skipped/filtered counts, active orders, duration, and throughput. A malformed record may contribute bytes without contributing a decoded-message count. A structurally decoded message with an invalid transition is counted before it fails. Unsupported and filtered are disjoint counters. Filtered counts only order messages excluded by symbol selection.

## Ownership and data structures

The book contains a global `unordered_map<uint64_t, Order>` and per-instrument bid/ask `std::map` price levels. Orders record locate, side, remaining shares, and price, with an internal link to the owning stable price-level node. Validation returns the order iterator and reductions reuse it; partial reductions avoid repeated hash, instrument, and price-tree lookups. Each level aggregates share quantity in 64 bits and records an order count. Bid levels are read in descending order; asks in ascending order. Reference lookup is average O(1); creating/removing a price level is O(log L). Producing depth is O(min(N,L)) plus instrument lookup. Memory is O(active orders + active levels + instruments).

The registry uses ordered stock locates for ownership and deterministic output, plus a growable vector indexed by the 16-bit locate for O(1) hot lookup. It uses at most 65,536 pointer slots. Book and registry copy operations rebuild their internal links to preserve independent ownership. Directory entries retain symbol, market category, financial status, and round-lot size; trading actions retain state and reason. The last system event is exposed as session state. Multiple start-of-messages events are rejected instead of silently mixing sessions. The application does not enforce a full exchange session-state machine.

Each cancellation or execution reduces the referenced displayed order at its original book price. An execution-with-price retains the execution price as event data; it does not move remaining shares to that price. Replacements validate the old reference, new reference, and quantity before removing the old order and inserting the replacement. Exhausted orders and empty price levels disappear.

## Error policy

Semantic validation failures leave the book unchanged. Allocation failure is fatal and is not covered by a transactional rollback guarantee. A fatal run must be discarded rather than resumed. Unsupported message payloads are not semantically validated; known unsupported types have their lengths checked. No implicit repair, reference inference, timestamp reordering, or missing-message recovery occurs.

Input, report, and snapshot paths must resolve to distinct files, including symlink and existing hard-link aliases. Existing output files are overwritten. Report and CSV writes use stream exceptions. Outputs are not transactionally committed; consult exit status and report completeness before consuming them.

## Extension points

A future network framer can feed the same decoder while owning session and sequence recovery. Profiling may justify replacing the reference hash table or level maps, provided the reference-model tests continue to pass. Snapshot persistence, full administrative metadata, complete trade analytics, and multi-session replay need explicit additional semantics.
