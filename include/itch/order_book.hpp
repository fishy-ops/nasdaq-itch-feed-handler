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
    std::unordered_map<std::uint64_t, Order> orders_;
    std::map<std::uint16_t, Sides> books_;
    void insert(std::uint64_t id, const Order &order);
    void reduce(std::uint64_t id, std::uint32_t quantity);

  public:
    void apply(const Event &event);
    std::vector<Level> depth(std::uint16_t locate, char side, std::size_t count) const;
    std::size_t active_orders() const {
        return orders_.size();
    }
};
} // namespace itch
