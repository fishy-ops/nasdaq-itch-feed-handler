#include "common.hpp"
#include "itch/replay.hpp"
#include <type_traits>

namespace {
void digest(comparison::Digest &d, const itch::Event &e) {
    d.number(static_cast<unsigned char>(e.header.type));
    d.number(e.header.locate);
    d.number(e.header.tracking);
    d.number(e.header.timestamp);
    std::visit(
        [&](const auto &v) {
            using T = std::decay_t<decltype(v)>;
            if constexpr (std::is_same_v<T, itch::System>)
                d.number(static_cast<unsigned char>(v.code));
            else if constexpr (std::is_same_v<T, itch::Directory>) {
                d.text(v.symbol);
                d.number(static_cast<unsigned char>(v.market));
                d.number(static_cast<unsigned char>(v.financial));
                d.number(v.round_lot);
            } else if constexpr (std::is_same_v<T, itch::TradingAction>) {
                d.text(v.symbol);
                d.number(static_cast<unsigned char>(v.state));
                d.text(v.reason);
            } else if constexpr (std::is_same_v<T, itch::Add>) {
                d.number(v.id);
                d.number(static_cast<unsigned char>(v.side));
                d.number(v.quantity);
                d.number(v.price);
                d.text(v.symbol);
                d.text(v.participant);
            } else if constexpr (std::is_same_v<T, itch::Reduce>) {
                d.number(v.id);
                d.number(v.quantity);
                d.number(v.match);
                d.number(static_cast<unsigned char>(v.printable));
                d.number(v.execution_price);
            } else if constexpr (std::is_same_v<T, itch::Delete>)
                d.number(v.id);
            else if constexpr (std::is_same_v<T, itch::Replace>) {
                d.number(v.old_id);
                d.number(v.new_id);
                d.number(v.quantity);
                d.number(v.price);
            } else
                throw std::runtime_error("comparison requires the shared supported subset");
        },
        e.payload);
}
struct Engine {
    static constexpr auto name = "fishy-ops";
    static comparison::Result process(const std::vector<char> &bytes, bool with_book, bool verify) {
        comparison::Result result;
        itch::Replay replay;
        std::size_t offset = 0;
        while (offset < bytes.size()) {
            if (bytes.size() - offset < 2)
                throw itch::Error(offset, 0, "truncated prefix");
            const auto size = (static_cast<unsigned char>(bytes[offset]) << 8) |
                              static_cast<unsigned char>(bytes[offset + 1]);
            if (size == 0 || bytes.size() - offset - 2 < static_cast<std::size_t>(size))
                throw itch::Error(offset, 0, "invalid frame length");
            const auto *payload = reinterpret_cast<const std::uint8_t *>(bytes.data() + offset + 2);
            auto event = itch::decode({payload, static_cast<std::size_t>(size)}, offset);
            ++result.messages;
            result.sink += event.header.timestamp + static_cast<unsigned char>(event.header.type);
            if (verify)
                digest(result.digest, event);
            if (with_book)
                replay.process(event, offset);
            offset += static_cast<std::size_t>(size) + 2;
        }
        if (verify)
            for (const auto &[loc, instrument] : replay.registry.all()) {
                (void)instrument;
                for (char side : {'B', 'S'})
                    for (const auto &l :
                         replay.book.depth(loc, side, std::numeric_limits<std::size_t>::max()))
                        result.levels.push_back(
                            {loc, static_cast<unsigned char>(side), l.price, l.quantity, l.orders});
            }
        return result;
    }
};
} // namespace
int main(int argc, char **argv) {
    return comparison::main<Engine>(argc, argv);
}
