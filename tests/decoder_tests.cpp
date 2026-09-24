#include "fixture.hpp"
#include "itch/decoder.hpp"
#include "itch/reader.hpp"
#include <gtest/gtest.h>
#include <sstream>
TEST(Decoder, IndependentGoldenBytes) {
    // Literal bytes independently transcribed from the A-message field layout.
    const std::vector<std::uint8_t> bytes = {65, 0,  1,  0,  2,  0,  0,  0,  0, 0, 3,  0,
                                             0,  0,  0,  0,  0,  0,  4,  66, 0, 0, 0,  5,
                                             65, 65, 80, 76, 32, 32, 32, 32, 0, 0, 39, 16};
    auto e = itch::decode(bytes);
    auto a = std::get<itch::Add>(e.payload);
    EXPECT_EQ(e.header.locate, 1);
    EXPECT_EQ(e.header.tracking, 2);
    EXPECT_EQ(e.header.timestamp, 3);
    EXPECT_EQ(a.id, 4);
    EXPECT_EQ(a.quantity, 5);
    EXPECT_EQ(a.price, 10000);
    EXPECT_EQ(a.symbol, "AAPL");
    EXPECT_EQ(a.side, 'B');
}
TEST(Decoder, AllSupportedFields) {
    auto b = fixture::add(0xFEDCBA9876543210ULL, 'S', 123, 1500123, 9, "ABCDEFGH", true);
    auto e = itch::decode(b);
    auto a = std::get<itch::Add>(e.payload);
    EXPECT_EQ(a.id, 0xFEDCBA9876543210ULL);
    EXPECT_EQ(a.participant, "TEST");
    EXPECT_EQ(a.symbol, "ABCDEFGH");
    EXPECT_EQ(a.price, 1500123);
    EXPECT_EQ(e.header.timestamp, 34200000000000ULL);
    auto d = std::get<itch::Directory>(itch::decode(fixture::directory(1, "AAPL")).payload);
    EXPECT_EQ(d.round_lot, 100);
    EXPECT_EQ(d.market, 'Q');
    EXPECT_EQ(d.financial, 'N');
    EXPECT_EQ(std::get<itch::System>(itch::decode(fixture::system('O')).payload).code, 'O');
    EXPECT_EQ(
        std::get<itch::TradingAction>(itch::decode(fixture::trading(1, "AAPL", 'H')).payload).state,
        'H');
    for (char t : {'E', 'C', 'X'}) {
        auto r = std::get<itch::Reduce>(itch::decode(fixture::reduce(t, 42, 7)).payload);
        EXPECT_EQ(r.id, 42);
        EXPECT_EQ(r.quantity, 7);
        EXPECT_EQ(r.match, t == 'X' ? 0 : 999);
        if (t == 'C') {
            EXPECT_EQ(r.printable, 'Y');
            EXPECT_EQ(r.execution_price, 1500200);
        }
    }
    auto u = std::get<itch::Replace>(itch::decode(fixture::replace(1, 2, 3, 4)).payload);
    EXPECT_EQ(u.old_id, 1);
    EXPECT_EQ(u.new_id, 2);
    EXPECT_EQ(u.quantity, 3);
    EXPECT_EQ(u.price, 4);
    EXPECT_EQ(std::get<itch::Delete>(itch::decode(fixture::erase(99)).payload).id, 99);
}
TEST(Decoder, RejectsEveryTruncationAndExtraByte) {
    for (auto b : fixture::demo()) {
        for (std::size_t n = 0; n < b.size(); ++n)
            EXPECT_THROW(itch::decode(std::span(b).first(n)), itch::Error);
        b.push_back(0);
        EXPECT_THROW(itch::decode(b), itch::Error);
    }
}
TEST(Decoder, ValidatesFieldsAndOffset) {
    auto b = fixture::add(1, 'Z', 1, 1);
    try {
        itch::decode(b, 123);
        FAIL();
    } catch (const itch::Error &e) {
        EXPECT_EQ(e.offset, 123);
        EXPECT_EQ(e.type, 'A');
    }
    b = fixture::add(1, 'B', 1, 1);
    fixture::number(b, 5, 6, 86400000000000ULL);
    EXPECT_THROW(itch::decode(b), itch::Error);
    b = fixture::add(1, 'B', 1, 1);
    b[24] = 0;
    EXPECT_THROW(itch::decode(b), itch::Error);
    b = fixture::reduce('C', 1, 1);
    b[31] = 'Z';
    EXPECT_THROW(itch::decode(b), itch::Error);

    EXPECT_THROW(itch::decode(fixture::system('Z')), itch::Error);
    EXPECT_THROW(itch::decode(fixture::trading(1, "AAPL", 'Z')), itch::Error);
    EXPECT_TRUE(
        std::holds_alternative<itch::Unsupported>(itch::decode(fixture::header('?', 11)).payload));
}
TEST(Reader, EmptyTruncatedAndZeroFrames) {
    itch::Frame f;
    std::istringstream empty;
    itch::Reader r(empty);
    EXPECT_FALSE(r.next(f));
    for (const std::string &s :
         {std::string("\0", 1), std::string("\0\0", 2), std::string("\0\x0cS", 3)}) {
        std::istringstream in(s);
        itch::Reader reader(in);
        EXPECT_THROW(reader.next(f), itch::Error);
    }
}
TEST(Reader, OffsetsAndReuse) {
    std::ostringstream out;
    fixture::write(out, fixture::system('O'));
    fixture::write(out, fixture::system('C'));
    std::istringstream in(out.str());
    itch::Reader reader(in);
    itch::Frame f;
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.offset, 0);
    ASSERT_TRUE(reader.next(f));
    EXPECT_EQ(f.offset, 14);
    EXPECT_EQ(reader.bytes_read(), 28);
    EXPECT_FALSE(reader.next(f));
}
