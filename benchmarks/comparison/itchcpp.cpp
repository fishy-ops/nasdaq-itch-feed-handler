#include "common.hpp"
#include "itch/book/book_manager.hpp"
#include "itch/parser.hpp"
#include <set>
#include <type_traits>

namespace {
template <class T> void digest(comparison::Digest &d, const T &v) {
    d.number(static_cast<unsigned char>(v.message_type));
    d.number(v.stock_locate);
    d.number(v.tracking_number);
    d.number(v.timestamp);
    if constexpr (std::is_same_v<T, itch::SystemEventMessage>)
        d.number(static_cast<unsigned char>(v.event_code));
    else if constexpr (std::is_same_v<T, itch::StockDirectoryMessage>) {
        d.text({v.stock, 8});
        d.number(static_cast<unsigned char>(v.market_category));
        d.number(static_cast<unsigned char>(v.financial_status_indicator));
        d.number(v.round_lot_size);
    } else if constexpr (std::is_same_v<T, itch::StockTradingActionMessage>) {
        d.text({v.stock, 8});
        d.number(static_cast<unsigned char>(v.trading_state));
        d.text({v.reason, 4});
    } else if constexpr (std::is_same_v<T, itch::AddOrderMessage> ||
                         std::is_same_v<T, itch::AddOrderMPIDAttributionMessage>) {
        d.number(v.order_reference_number);
        d.number(static_cast<unsigned char>(v.buy_sell_indicator));
        d.number(v.shares);
        d.number(v.price);
        d.text({v.stock, 8});
        if constexpr (std::is_same_v<T, itch::AddOrderMPIDAttributionMessage>)
            d.text({v.attribution, 4});
        else
            d.text("");
    } else if constexpr (std::is_same_v<T, itch::OrderExecutedMessage> ||
                         std::is_same_v<T, itch::OrderExecutedWithPriceMessage> ||
                         std::is_same_v<T, itch::OrderCancelMessage>) {
        d.number(v.order_reference_number);
        if constexpr (std::is_same_v<T, itch::OrderCancelMessage>) {
            d.number(v.cancelled_shares);
            d.number(0);
        } else {
            d.number(v.executed_shares);
            d.number(v.match_number);
        }
        if constexpr (std::is_same_v<T, itch::OrderExecutedWithPriceMessage>) {
            d.number(static_cast<unsigned char>(v.printable));
            d.number(v.execution_price);
        } else {
            d.number(0);
            d.number(0);
        }
    } else if constexpr (std::is_same_v<T, itch::OrderDeleteMessage>)
        d.number(v.order_reference_number);
    else if constexpr (std::is_same_v<T, itch::OrderReplaceMessage>) {
        d.number(v.original_order_reference_number);
        d.number(v.new_order_reference_number);
        d.number(v.shares);
        d.number(v.price);
    } else
        throw std::runtime_error("comparison requires the shared supported subset");
}
struct Engine {
    static constexpr auto name = "bbalouki-itchcpp";
    static comparison::Result process(const std::vector<char> &bytes, bool with_book, bool verify) {
        comparison::Result result;
        itch::Parser parser;
        itch::book::BookManager manager;
        std::set<std::uint16_t> locates;
        parser.parse(bytes.data(), bytes.size(), [&](const itch::Message &message) {
            ++result.messages;
            std::visit(
                [&](const auto &v) {
                    result.sink += v.timestamp + static_cast<unsigned char>(v.message_type);
                    if (verify) {
                        digest(result.digest, v);
                        if (v.stock_locate)
                            locates.insert(v.stock_locate);
                    }
                },
                message);
            if (with_book)
                manager.process(message);
        });
        if (verify)
            for (auto loc : locates)
                if (auto book = manager.book(loc)) {
                    for (auto side : {itch::book::Side::buy, itch::book::Side::sell})
                        for (const auto &l : book->depth(side))
                            result.levels.push_back({loc, static_cast<unsigned char>(side),
                                                     l.price.raw(), l.shares, l.order_count});
                }
        return result;
    }
};
} // namespace
int main(int argc, char **argv) {
    return comparison::main<Engine>(argc, argv);
}
