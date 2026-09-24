#include "itch/replay.hpp"
#include <charconv>
#include <chrono>
#include <fstream>
#include <iostream>
#include <sys/resource.h>
#include <sys/utsname.h>
int main(int argc, char **argv) {
    try {
        if (argc < 2 || argc > 3)
            throw std::runtime_error("usage: itch_benchmark FILE [ITERATIONS]");
        unsigned iterations = 5;
        if (argc == 3) {
            std::string s = argv[2];
            auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), iterations);
            if (ec != std::errc{} || p != s.data() + s.size() || !iterations)
                throw std::runtime_error("invalid iterations");
        }
        std::ifstream in(argv[1], std::ios::binary);
        if (!in)
            throw std::runtime_error("cannot open input");
        itch::Reader reader(in);
        itch::Frame f;
        std::vector<itch::Frame> frames;
        std::uint64_t hash = 14695981039346656037ULL;
        while (reader.next(f)) {
            for (auto b : f.bytes) {
                hash ^= b;
                hash *= 1099511628211ULL;
            }
            frames.push_back(std::move(f));
        }
        // Untimed validation/warmup prevents benchmarking an invalid event stream.
        {
            itch::Replay warmup;
            for (const auto &frame : frames)
                warmup.process(itch::decode(frame.bytes, frame.offset), frame.offset);
        }
        struct utsname host{};
        uname(&host);
        std::cout << "compiler=" << ITCH_COMPILER << " build_type=" << ITCH_BUILD_TYPE
                  << " host=" << host.sysname << ' ' << host.machine << " dataset=" << argv[1]
                  << " payload_fnv1a64=" << std::hex << hash << std::dec
                  << " bytes=" << reader.bytes_read() << " messages=" << frames.size()
                  << " iterations=" << iterations << '\n';
        std::cout << "mode,iteration,messages,elapsed_seconds,messages_per_second,checksum\n";
        for (bool with_book : {false, true})
            for (unsigned run = 0; run < iterations; ++run) {
                itch::Replay replay;
                std::uint64_t checksum = 0;
                auto start = std::chrono::steady_clock::now();
                for (const auto &frame : frames) {
                    auto event = itch::decode(frame.bytes, frame.offset);
                    checksum +=
                        event.header.timestamp + static_cast<unsigned char>(event.header.type);
                    if (with_book)
                        replay.process(event, frame.offset);
                }
                auto elapsed =
                    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
                checksum += replay.book.active_orders();
                std::cout << (with_book ? "decode_and_book" : "decode") << ',' << run << ','
                          << frames.size() << ',' << elapsed << ','
                          << (elapsed > 0 ? static_cast<double>(frames.size()) / elapsed : 0) << ','
                          << checksum << '\n';
            }
        struct rusage usage{};
        getrusage(RUSAGE_SELF, &usage);
#ifdef __APPLE__
        const auto peak_bytes = usage.ru_maxrss;
#else
        const auto peak_bytes = usage.ru_maxrss * 1024L;
#endif
        std::cout << "peak_rss_bytes=" << peak_bytes << " (includes preloaded frames and warmup)\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
