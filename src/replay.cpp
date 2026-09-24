#include "itch/replay.hpp"
#include <chrono>
#include <iomanip>
namespace itch {
void Replay::process(const Event &e, std::uint64_t offset) {
    ++metrics.messages;
    ++metrics.by_type[e.header.type];
    try {
        if (std::holds_alternative<Unsupported>(e.payload)) {
            ++metrics.unsupported;
            return;
        }
        registry.apply(e);
        const bool order =
            std::holds_alternative<Add>(e.payload) || std::holds_alternative<Reduce>(e.payload) ||
            std::holds_alternative<Delete>(e.payload) || std::holds_alternative<Replace>(e.payload);
        if (!order)
            return;
        const auto &i = registry.at(e.header.locate);
        if (auto a = std::get_if<Add>(&e.payload); a && a->symbol != i.directory.symbol)
            throw std::runtime_error("add symbol mismatch");
        if (!options_.symbol.empty() && i.directory.symbol != options_.symbol) {
            ++metrics.filtered;
            return;
        }
        book.apply(e);
    } catch (const std::exception &ex) {
        throw Error(offset, e.header.type, ex.what());
    }
}
void Replay::run(std::istream &input, std::ostream *snapshots) {
    if (snapshots && !options_.snapshot_every)
        throw std::invalid_argument("snapshot interval must be positive");
    Reader reader(input);
    Frame f;
    const auto start = std::chrono::steady_clock::now();
    auto finish = [&] {
        metrics.bytes = reader.bytes_read();
        metrics.elapsed_seconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    };
    try {
        if (snapshots)
            *snapshots << "message,symbol,state,side,rank,price,quantity,orders\n";
        while (reader.next(f)) {
            process(decode(f.bytes, f.offset), f.offset);
            if (snapshots && metrics.messages % options_.snapshot_every == 0)
                write_depth(*snapshots, true);
        }
        if (!options_.symbol.empty()) {
            bool found = false;
            for (const auto &[loc, i] : registry.all()) {
                (void)loc;
                found |= i.directory.symbol == options_.symbol;
            }
            if (!found)
                throw std::runtime_error("requested symbol absent from directory");
        }
        finish();
    } catch (...) {
        ++metrics.errors;
        finish();
        throw;
    }
}
void Replay::write_depth(std::ostream &o, bool csv) const {
    for (const auto &[loc, i] : registry.all()) {
        if (!options_.symbol.empty() && options_.symbol != i.directory.symbol)
            continue;
        if (!csv)
            o << i.directory.symbol << " state=" << i.state << " session=" << registry.session()
              << '\n';
        for (char side : {'B', 'S'}) {
            auto levels = book.depth(loc, side, options_.depth);
            if (!csv && levels.empty())
                o << (side == 'B' ? "BID" : "ASK") << " empty\n";
            std::size_t rank = 0;
            for (const auto &l : levels) {
                ++rank;
                if (csv) {
                    o << metrics.messages << ",\"";
                    for (char c : i.directory.symbol) {
                        if (c == '"')
                            o << '"';
                        o << c;
                    }
                    o << "\"," << i.state << ',' << side << ',' << rank << ',';
                } else
                    o << (side == 'B' ? "BID" : "ASK") << ' ' << rank << ' ';
                o << l.price / 10000 << '.' << std::setw(4) << std::setfill('0') << l.price % 10000
                  << std::setfill(' ') << (csv ? "," : " qty=") << l.quantity
                  << (csv ? "," : " orders=") << l.orders << '\n';
            }
        }
    }
}
} // namespace itch
