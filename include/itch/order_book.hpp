#pragma once
#include "itch/events.hpp"
#include <map>
#include <unordered_map>
#include <vector>
namespace itch {
struct Level {
    std::uint32_t price{};
    std::uint64_t quantity{}, orders{};
    bool operator==(const Level &) const = default;
};
struct Order {
    std::uint16_t locate{};
    char side{};
    std::uint32_t quantity{}, price{};
};
class OrderBook {
    struct Sides {
        std::map<std::uint32_t, Level> bids, asks;
    };
    // std::map nodes remain stable until erased. A live order owns a share
    // of its level, so this pointer cannot dangle while the order is active.
    struct StoredOrder {
        Order order;
        Level *level;
    };
    using Orders = std::unordered_map<std::uint64_t, StoredOrder>;
    Orders orders_;
    std::map<std::uint16_t, Sides> books_;
    void insert(std::uint64_t id, const Order &order);
    void reduce(Orders::iterator order, std::uint32_t quantity);

  public:
    OrderBook() = default;
    OrderBook(const OrderBook &other);
    OrderBook &operator=(const OrderBook &other);
    OrderBook(OrderBook &&) noexcept = default;
    OrderBook &operator=(OrderBook &&) noexcept = default;
    void apply(const Event &event);
    std::vector<Level> depth(std::uint16_t locate, char side, std::size_t count) const;
    std::size_t active_orders() const {
        return orders_.size();
    }
};
} // namespace itch
