#pragma once
#include "itch/decoder.hpp"
#include "itch/instrument_registry.hpp"
#include "itch/metrics.hpp"
#include "itch/order_book.hpp"
#include "itch/reader.hpp"
namespace itch {
struct ReplayOptions {
    std::string symbol;
    std::size_t depth{5};
    std::uint64_t snapshot_every{100000};
};
class Replay {
    ReplayOptions options_;

  public:
    InstrumentRegistry registry;
    OrderBook book;
    Metrics metrics;
    explicit Replay(ReplayOptions options = {}) : options_(std::move(options)) {}
    void process(const Event &event, std::uint64_t offset = 0);
    void run(std::istream &input, std::ostream *snapshots = nullptr);
    void write_depth(std::ostream &output, bool csv = false) const;
};
} // namespace itch
