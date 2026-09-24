#include "itch/order_book.hpp"
#include <stdexcept>
namespace itch {
OrderBook::OrderBook(const OrderBook &other) {
    // Rebuild internal links; copying raw level pointers would alias the source.
    for (const auto &[id, stored] : other.orders_)
        insert(id, stored.order);
}
OrderBook &OrderBook::operator=(const OrderBook &other) {
    if (this != &other) {
        OrderBook copy(other);
        orders_.swap(copy.orders_);
        books_.swap(copy.books_);
    }
    return *this;
}
void OrderBook::insert(std::uint64_t id, const Order &o) {
    auto &sides = books_[o.locate];
    auto &levels = o.side == 'B' ? sides.bids : sides.asks;
    auto &l = levels[o.price];
    l.price = o.price;
    l.quantity += o.quantity;
    ++l.orders;
    orders_.emplace(id, StoredOrder{o, &l});
}
void OrderBook::reduce(Orders::iterator it, std::uint32_t q) {
    auto &o = it->second.order;
    auto &l = *it->second.level;
    l.quantity -= q;
    o.quantity -= q;
    if (o.quantity == 0) {
        --l.orders;
        if (l.orders == 0) {
            // Only the final order leaving a level needs a price-tree lookup.
            auto &sides = books_.at(o.locate);
            auto &levels = o.side == 'B' ? sides.bids : sides.asks;
            levels.erase(o.price);
        }
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
    auto existing = [&](std::uint64_t id) -> Orders::iterator {
        auto it = orders_.find(id);
        if (it == orders_.end())
            throw std::runtime_error("missing order reference");
        if (it->second.order.locate != e.header.locate)
            throw std::runtime_error("order locate mismatch");
        return it;
    };
    if (auto a = std::get_if<Add>(&e.payload)) {
        valid(a->quantity, a->price);
        if (a->side != 'B' && a->side != 'S')
            throw std::runtime_error("invalid side");
        if (orders_.contains(a->id))
            throw std::runtime_error("duplicate active order reference");
        insert(a->id, {e.header.locate, a->side, a->quantity, a->price});
    } else if (auto r = std::get_if<Reduce>(&e.payload)) {
        auto it = existing(r->id);
        if (!r->quantity || r->quantity > it->second.order.quantity)
            throw std::runtime_error("invalid reduction quantity");
        reduce(it, r->quantity);
    } else if (auto d = std::get_if<Delete>(&e.payload)) {
        auto it = existing(d->id);
        reduce(it, it->second.order.quantity);
    } else if (auto u = std::get_if<Replace>(&e.payload)) {
        auto it = existing(u->old_id);
        const auto o = it->second.order;
        valid(u->quantity, u->price);
        if (orders_.contains(u->new_id))
            throw std::runtime_error("replacement reference already active");
        reduce(it, o.quantity);
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
