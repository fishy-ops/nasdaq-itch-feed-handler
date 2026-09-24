#include "itch/replay.hpp"
#include <charconv>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
namespace {
std::uint64_t positive(const std::string &s) {
    std::uint64_t n{};
    auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), n);
    if (ec != std::errc{} || p != s.data() + s.size() || !n)
        throw std::runtime_error("expected positive integer: " + s);
    return n;
}
void distinct(const std::string &a, const std::string &b) {
    if (a.empty() || b.empty())
        return;
    auto x = std::filesystem::weakly_canonical(a), y = std::filesystem::weakly_canonical(b);
    if (x == y || (std::filesystem::exists(x) && std::filesystem::exists(y) &&
                   std::filesystem::equivalent(x, y)))
        throw std::runtime_error("input and output paths must be distinct");
}
} // namespace
int main(int argc, char **argv) {
    try {
        std::string input, report, snapshot;
        itch::ReplayOptions options;
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--help") {
                std::cout << "itch_replay --input FILE [--symbol SYMBOL] [--depth N] [--report "
                             "FILE] [--snapshots FILE --snapshot-every N]\n";
                return 0;
            }
            if (arg != "--input" && arg != "--report" && arg != "--snapshots" &&
                arg != "--symbol" && arg != "--depth" && arg != "--snapshot-every")
                throw std::runtime_error("unknown option: " + arg);
            if (++i == argc)
                throw std::runtime_error("missing value for " + arg);
            std::string value = argv[i];
            if (arg == "--input")
                input = value;
            else if (arg == "--report")
                report = value;
            else if (arg == "--snapshots")
                snapshot = value;
            else if (arg == "--symbol")
                options.symbol = value;
            else if (arg == "--depth") {
                auto n = positive(value);
                if (n > std::numeric_limits<std::size_t>::max())
                    throw std::runtime_error("depth too large");
                options.depth = static_cast<std::size_t>(n);
            } else
                options.snapshot_every = positive(value);
        }
        if (input.empty())
            throw std::runtime_error("--input is required; use --help");
        distinct(input, report);
        distinct(input, snapshot);
        distinct(report, snapshot);
        std::ifstream in(input, std::ios::binary);
        if (!in)
            throw std::runtime_error("cannot open input: " + input);
        std::ofstream snapshots;
        if (!snapshot.empty()) {
            snapshots.exceptions(std::ios::badbit | std::ios::failbit);
            snapshots.open(snapshot);
        }
        itch::Replay replay(options);
        std::string error;
        try {
            replay.run(in, snapshot.empty() ? nullptr : &snapshots);
            if (snapshots.is_open())
                snapshots.flush();
        } catch (const std::exception &ex) {
            error = ex.what();
            if (!replay.metrics.errors)
                ++replay.metrics.errors;
        }
        if (!report.empty()) {
            std::ofstream out;
            out.exceptions(std::ios::badbit | std::ios::failbit);
            out.open(report);
            replay.metrics.write_json(out, input, error, replay.book.active_orders());
            out.close();
        }
        if (!error.empty()) {
            std::cerr << error << '\n';
            return 1;
        }
        replay.write_depth(std::cout);
        std::cout << "messages=" << replay.metrics.messages
                  << " unsupported=" << replay.metrics.unsupported
                  << " active_orders=" << replay.book.active_orders()
                  << " elapsed_seconds=" << replay.metrics.elapsed_seconds << '\n';
        return 0;
    } catch (const std::exception &ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }
}
