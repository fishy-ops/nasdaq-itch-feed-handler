#include "fixture.hpp"
#include "itch/decoder.hpp"
#include "itch/order_book.hpp"
#include <algorithm>
#include <gtest/gtest.h>
#include <random>
using itch::Level;
TEST(Book, LifecycleAndPriceOrdering) {
    itch::OrderBook b;
    auto apply = [&](auto bytes) { b.apply(itch::decode(bytes)); };
    apply(fixture::add(1, 'B', 100, 100));
    apply(fixture::add(2, 'B', 50, 100));
    apply(fixture::add(3, 'B', 10, 99));
    apply(fixture::add(4, 'S', 20, 110));
    apply(fixture::add(5, 'S', 30, 109));
    EXPECT_EQ(b.depth(1, 'B', 1), (std::vector<Level>{{100, 150, 2}}));
    EXPECT_EQ(b.depth(1, 'S', 2), (std::vector<Level>{{109, 30, 1}, {110, 20, 1}}));
    apply(fixture::reduce('E', 1, 20));
    apply(fixture::reduce('C', 2, 10));
    apply(fixture::reduce('X', 1, 10));
    EXPECT_EQ(b.depth(1, 'B', 1), (std::vector<Level>{{100, 110, 2}}));
    apply(fixture::replace(1, 6, 60, 101));
    apply(fixture::erase(2));
    EXPECT_EQ(b.depth(1, 'B', 2), (std::vector<Level>{{101, 60, 1}, {99, 10, 1}}));
    apply(fixture::reduce('E', 6, 60));
    EXPECT_EQ(b.depth(1, 'B', 1), (std::vector<Level>{{99, 10, 1}}));
}
TEST(Book, InvalidEventsDoNotMutate) {
    itch::OrderBook b;
    b.apply(itch::decode(fixture::add(1, 'B', 10, 100)));
    b.apply(itch::decode(fixture::add(2, 'S', 10, 110)));
    const auto before = b.depth(1, 'B', 10);
    for (auto bytes : {fixture::add(1, 'B', 10, 100), fixture::add(3, 'B', 0, 100),
                       fixture::add(3, 'B', 1, 2000000001U), fixture::reduce('X', 1, 11),
                       fixture::reduce('X', 1, 0), fixture::erase(99), fixture::erase(1, 2),
                       fixture::replace(1, 2, 10, 100), fixture::replace(1, 3, 0, 100)}) {

        EXPECT_THROW(b.apply(itch::decode(bytes)), std::runtime_error);
        EXPECT_EQ(b.depth(1, 'B', 10), before);
        EXPECT_EQ(b.active_orders(), 2);
    }
}
TEST(Book, GeneratedSequencesMatchIndependentReference) {
    itch::OrderBook book;
    std::map<std::uint64_t, itch::Order> reference;
    std::mt19937 rng(2026);
    std::uint64_t next = 1;
    auto verify = [&] {
        for (std::uint16_t loc : {std::uint16_t{1}, std::uint16_t{2}})
            for (char side : {'B', 'S'}) {
                std::map<std::uint32_t, Level> totals;
                for (auto [id, o] : reference) {
                    (void)id;
                    if (o.locate == loc && o.side == side) {
                        auto &l = totals[o.price];
                        l.price = o.price;
                        l.quantity += o.quantity;
                        ++l.orders;
                    }
                }
                std::vector<Level> expected;
                for (auto [price, l] : totals) {
                    (void)price;
                    expected.push_back(l);
                }
                if (side == 'B')
                    std::reverse(expected.begin(), expected.end());
                EXPECT_EQ(book.depth(loc, side, 1000), expected);
            }
        EXPECT_EQ(book.active_orders(), reference.size());
    };
    for (int step = 0; step < 5000; ++step) {
        auto op = rng() % 4;
        if (reference.empty() || op == 0) {
            auto id = next++;
            auto loc = static_cast<std::uint16_t>(1 + rng() % 2);
            itch::Order o{loc, rng() % 2 ? 'B' : 'S', 1 + rng() % 100, 100 + rng() % 20};
            book.apply({{'A', loc, 0, 0}, itch::Add{id, o.side, o.quantity, o.price, "", ""}});
            reference[id] = o;
        } else {
            auto it = reference.begin();
            std::advance(it, rng() % reference.size());
            auto id = it->first;
            auto o = it->second;
            if (op == 1) {
                auto q = 1 + rng() % o.quantity;
                book.apply({{'X', o.locate, 0, 0}, itch::Reduce{id, q, 0, 0, 0}});
                it->second.quantity -= q;
                if (!it->second.quantity)
                    reference.erase(it);
            } else if (op == 2) {
                book.apply({{'D', o.locate, 0, 0}, itch::Delete{id}});
                reference.erase(it);
            } else {
                auto new_id = next++;
                o.price = 100 + rng() % 20;
                o.quantity = 1 + rng() % 100;
                book.apply({{'U', o.locate, 0, 0}, itch::Replace{id, new_id, o.quantity, o.price}});
                reference.erase(it);
                reference[new_id] = o;
            }
        }
        verify();
    }
}

TEST(Book, LevelLinksSurviveRehashAndIndependentCopies) {
    itch::OrderBook original;
    // Enough IDs to force multiple hash rehashes, sharing levels across symbols.
    for (std::uint64_t id = 1; id <= 10000; ++id)
        original.apply(itch::decode(fixture::add(id, 'B', 100, 100, 1)));
    itch::OrderBook copy = original;
    itch::OrderBook assigned;
    assigned.apply(itch::decode(fixture::add(99999, 'S', 1, 900, 2)));
    assigned = original;
    for (std::uint64_t id = 1; id <= 10000; ++id) {
        original.apply(itch::decode(fixture::reduce('X', id, 1)));
        copy.apply(itch::decode(fixture::erase(id)));
    }
    EXPECT_TRUE(copy.depth(1, 'B', 1).empty());
    EXPECT_EQ(original.depth(1, 'B', 1), (std::vector<Level>{{100, 990000, 10000}}));
    EXPECT_EQ(assigned.depth(1, 'B', 1), (std::vector<Level>{{100, 1000000, 10000}}));
    EXPECT_TRUE(assigned.depth(2, 'S', 1).empty());
    auto *same = &assigned;
    assigned = *same;
    EXPECT_EQ(assigned.depth(1, 'B', 1), (std::vector<Level>{{100, 1000000, 10000}}));
    // Move construction/assignment must preserve links to the transferred maps.
    auto moved = std::move(assigned);
    moved.apply(itch::decode(fixture::reduce('E', 1, 50)));
    copy = std::move(moved);
    copy.apply(itch::decode(fixture::reduce('E', 1, 50)));
    EXPECT_EQ(copy.depth(1, 'B', 1), (std::vector<Level>{{100, 999900, 9999}}));
}

TEST(Book, LastOrderEraseAndSamePriceReplacementKeepLinksValid) {
    itch::OrderBook book;
    for (std::uint64_t id = 1; id <= 500; ++id) {
        book.apply(itch::decode(fixture::add(id * 2, 'B', 100, 100)));
        book.apply(itch::decode(fixture::replace(id * 2, id * 2 + 1, 75, 100)));
        book.apply(itch::decode(fixture::reduce('C', id * 2 + 1, 75)));
        EXPECT_EQ(book.active_orders(), 0);
        EXPECT_TRUE(book.depth(1, 'B', 1).empty());
    }
}
