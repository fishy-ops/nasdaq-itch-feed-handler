#include "itch/order_book.hpp"
#include <stdexcept>
namespace itch {
void OrderBook::insert(std::uint64_t id, const Order &o) {
    auto &sides = books_[o.locate];
    auto &levels = o.side == 'B' ? sides.bids : sides.asks;
    auto &l = levels[o.price];
    l.price = o.price;
    l.quantity += o.quantity;
    ++l.orders;
    orders_.emplace(id, o);
}
void OrderBook::reduce(std::uint64_t id, std::uint32_t q) {
    auto it = orders_.find(id);
    auto &o = it->second;
    auto &sides = books_.at(o.locate);
    auto &levels = o.side == 'B' ? sides.bids : sides.asks;
    auto &l = levels.at(o.price);
    l.quantity -= q;
    o.quantity -= q;
    if (o.quantity == 0) {
        --l.orders;
        if (l.orders == 0)
            levels.erase(o.price);
        orders_.erase(it);
    }
}
void OrderBook::apply(const Event &e) {
    auto valid = [](std::uint32_t q, std::uint32_t p) {
        if (!q)
            throw std::runtime_error("zero quantity");
        if (p > 2000000000U)
            throw std::runtime_error("price exceeds ITCH Price(4) maximum");
    };
    auto existing = [&](std::uint64_t id) -> Order {
        auto it = orders_.find(id);
        if (it == orders_.end())
            throw std::runtime_error("missing order reference");
        if (it->second.locate != e.header.locate)
            throw std::runtime_error("order locate mismatch");
        return it->second;
    };
    if (auto a = std::get_if<Add>(&e.payload)) {
        valid(a->quantity, a->price);
        if (a->side != 'B' && a->side != 'S')
            throw std::runtime_error("invalid side");
        if (orders_.contains(a->id))
            throw std::runtime_error("duplicate active order reference");
        insert(a->id, {e.header.locate, a->side, a->quantity, a->price});
    } else if (auto r = std::get_if<Reduce>(&e.payload)) {
        auto o = existing(r->id);
        if (!r->quantity || r->quantity > o.quantity)
            throw std::runtime_error("invalid reduction quantity");
        reduce(r->id, r->quantity);
    } else if (auto d = std::get_if<Delete>(&e.payload)) {
        auto o = existing(d->id);
        reduce(d->id, o.quantity);
    } else if (auto u = std::get_if<Replace>(&e.payload)) {
        auto o = existing(u->old_id);
        valid(u->quantity, u->price);
        if (orders_.contains(u->new_id))
            throw std::runtime_error("replacement reference already active");
        reduce(u->old_id, o.quantity);
        insert(u->new_id, {o.locate, o.side, u->quantity, u->price});
    }
}
std::vector<Level> OrderBook::depth(std::uint16_t locate, char side, std::size_t count) const {
    if (side != 'B' && side != 'S')
        throw std::invalid_argument("invalid depth side");
    std::vector<Level> result;
    auto it = books_.find(locate);
    if (it == books_.end())
        return result;
    if (side == 'B') {
        for (auto p = it->second.bids.rbegin();
             p != it->second.bids.rend() && result.size() < count; ++p)
            result.push_back(p->second);
    } else {
        for (const auto &[price, l] : it->second.asks) {
            (void)price;
            if (result.size() == count)
                break;
            result.push_back(l);
        }
    }
    return result;
}
} // namespace itch
