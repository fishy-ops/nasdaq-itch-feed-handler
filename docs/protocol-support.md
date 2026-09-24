# Protocol support

Reference: [Nasdaq TotalView-ITCH 5.0 specification](https://www.nasdaqtrader.com/content/technicalsupport/specifications/dataproducts/NQTVITCHspecification.pdf), retrieved September 24, 2026. The consulted PDF's Appendix A begins with the **April 28, 2023** revision. This pins the implemented layout reference, not a claim that no newer exchange changes exist.

Input uses unsigned big-endian fields. The common header has message type (1 byte), stock locate (2), tracking number (2), and nanoseconds since midnight (6). Length prefixes are file framing, not part of this header. Prices use four implied decimal places. The decoder checks timestamp range, supported message lengths, text encoding for retained text fields, and side/state/printable enums used by this application.

| Type | Payload bytes | Application behavior |
|---|---:|---|
| S | 12 | Retain system event |
| R | 39 | Register stock identity and selected metadata |
| H | 25 | Retain trading state and reason |
| A | 36 | Add displayed order |
| F | 40 | Add displayed order; decode participant attribution |
| E | 31 | Reduce displayed order after execution |
| C | 36 | Reduce displayed order; retain execution price and printable flag |
| X | 23 | Reduce order after cancellation |
| D | 19 | Remove complete remaining order |
| U | 35 | Replace reference, remaining quantity, and price |

Directory fields not needed for the documented application are not exposed or semantically validated. Participant attribution is available on Add events but not retained in the aggregate book. Execution match identifiers and printable flags are decoded but no trade tape is constructed. Symbols in additions and trading actions must match the registered stock locate.

Y, L, V, W, K, J, h, P, Q, B, I, N, and O messages are recognized for exact length validation and counted as unsupported. In particular, standalone trades and cross prints do not cause a second displayed-book decrement. Broken-trade events are counted without restoring orders. Unknown types are skipped after common-header validation. Check the report's per-type and unsupported counts when evaluating a new dataset.

The reader accepts clean empty input, rejects zero-length records, and fails on truncated prefixes/payloads. It cannot detect a lost but otherwise correctly framed record; source sequence guarantees belong to transport/replay provenance. Symbol selection does not bypass structural decoding of other records.

The committed demo is synthetic, contains no exchange-distributed market data, and is generated with `generate_fixture`. No historical-data availability or licensing entitlement is assumed.
