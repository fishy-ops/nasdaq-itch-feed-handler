#include "fixture.hpp"
#include "itch/replay.hpp"
#include <fstream>
#include <gtest/gtest.h>
#include <sstream>
TEST(Replay, CommittedFixtureAndDeterminism) {
    std::string prior;
    for (int run = 0; run < 2; ++run) {
        std::ifstream in(std::string(FIXTURE_DIR) + "/demo.itch", std::ios::binary);
        ASSERT_TRUE(in.good());
        itch::Replay r;
        r.run(in);
        EXPECT_EQ(r.metrics.messages, 16);
        EXPECT_EQ(r.metrics.unsupported, 1);
        EXPECT_EQ(r.metrics.errors, 0);
        EXPECT_EQ(r.book.active_orders(), 3);
        EXPECT_EQ(r.book.depth(1, 'B', 5), (std::vector<itch::Level>{{1500100, 70, 1}}));
        EXPECT_EQ(r.book.depth(1, 'S', 5), (std::vector<itch::Level>{{1500500, 110, 1}}));
        EXPECT_EQ(r.book.depth(2, 'B', 5), (std::vector<itch::Level>{{3000000, 80, 1}}));
        EXPECT_EQ(r.registry.session(), 'C');
        std::ostringstream out;
        r.write_depth(out);
        if (run)
            EXPECT_EQ(out.str(), prior);
        prior = out.str();
    }
}
TEST(Replay, FilterAndSnapshots) {
    std::ifstream in(std::string(FIXTURE_DIR) + "/demo.itch", std::ios::binary);
    itch::Replay r({"AAPL", 5, 8});
    std::ostringstream snapshots;
    r.run(in, &snapshots);
    EXPECT_EQ(r.book.active_orders(), 2);
    EXPECT_EQ(r.metrics.filtered, 1);
    EXPECT_EQ(snapshots.str().find("MSFT"), std::string::npos);
    EXPECT_NE(snapshots.str().find("16,\"AAPL\",T,B,1,150.0100,70,1"), std::string::npos);
}
TEST(Replay, EmptyAndMissingSymbol) {
    std::istringstream in;
    itch::Replay r;
    r.run(in);
    EXPECT_EQ(r.metrics.messages, 0);
    std::istringstream empty;
    itch::Replay selected({"AAPL", 5, 1});
    EXPECT_THROW(selected.run(empty), std::runtime_error);
}
TEST(Replay, ContextualTransitionFailure) {
    std::ostringstream data;
    fixture::write(data, fixture::directory(1, "AAPL"));
    fixture::write(data, fixture::erase(999));
    std::istringstream in(data.str());
    itch::Replay r;
    try {
        r.run(in);
        FAIL();
    } catch (const itch::Error &e) {
        EXPECT_EQ(e.offset, 41);
        EXPECT_EQ(e.type, 'D');
    }
    EXPECT_EQ(r.metrics.errors, 1);
    EXPECT_EQ(r.metrics.bytes, data.str().size());
}
TEST(Replay, RegistryValidation) {
    itch::Replay r;
    r.process(itch::decode(fixture::directory(1, "AAPL")));
    EXPECT_THROW(r.process(itch::decode(fixture::directory(1, "MSFT"))), itch::Error);
    EXPECT_THROW(r.process(itch::decode(fixture::directory(2, "AAPL"))), itch::Error);
    EXPECT_THROW(r.process(itch::decode(fixture::trading(1, "MSFT"))), itch::Error);
    EXPECT_THROW(r.process(itch::decode(fixture::add(1, 'B', 1, 1, 1, "MSFT"))), itch::Error);
    EXPECT_EQ(r.book.active_orders(), 0);
}
TEST(Replay, MissingDirectoryAndMultipleSessions) {
    itch::Replay r;
    EXPECT_THROW(r.process(itch::decode(fixture::add(1, 'B', 1, 1))), itch::Error);
    r.process(itch::decode(fixture::system('O')));
    EXPECT_THROW(r.process(itch::decode(fixture::system('O'))), itch::Error);
}
TEST(Replay, FixtureGeneratorMatchesCommittedBytes) {
    std::ostringstream generated;
    for (auto b : fixture::demo())
        fixture::write(generated, b);
    std::ifstream in(std::string(FIXTURE_DIR) + "/demo.itch", std::ios::binary);
    std::string disk((std::istreambuf_iterator<char>(in)), {});
    EXPECT_EQ(disk, generated.str());
}
