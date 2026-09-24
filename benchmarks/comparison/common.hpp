#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <vector>

namespace comparison {
// Shared value-level digest: covers every field retained by fishy-ops' supported
// event types. Text is unpadded ASCII. Overflow is intentional modulo 2^64.
struct Digest {
    std::uint64_t value{14695981039346656037ULL};
    void number(std::uint64_t n) {
        value = (value ^ n) * 1099511628211ULL;
    }
    void text(std::string_view s) {
        while (!s.empty() && s.back() == ' ')
            s.remove_suffix(1);
        number(s.size());
        for (char c : s)
            number(static_cast<unsigned char>(c));
    }
};
using Level = std::array<std::uint64_t, 5>; // locate, ASCII side, price, shares, count
struct Result {
    std::uint64_t messages{}, sink{};
    Digest digest;
    std::vector<Level> levels;
};
inline std::vector<char> load(const char *path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("cannot open input");
    auto size = file.tellg();
    if (size < 0)
        throw std::runtime_error("cannot size input");
    std::vector<char> bytes(static_cast<std::size_t>(size));
    file.seekg(0);
    if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("cannot read input");
    return bytes;
}
inline long peak_rss() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0)
        throw std::runtime_error("getrusage failed");
#ifdef __APPLE__
    return usage.ru_maxrss;
#else
    return usage.ru_maxrss * 1024L;
#endif
}
// Engine supplies process(bytes, book, verify). Validation and depth extraction
// run outside all timed regions. Timed engines are destroyed at return: both
// measurements include construction/destruction, counters, and a header sink.
template <class Engine> int main(int argc, char **argv) {
    try {
        if (argc != 3)
            throw std::runtime_error("usage: EXECUTABLE INPUT decode|book|verify");
        std::string mode = argv[2];
        if (mode != "decode" && mode != "book" && mode != "verify")
            throw std::runtime_error("invalid mode");
        const auto bytes = load(argv[1]);
        Result result;
        double seconds = 0;
        if (mode == "verify")
            result = Engine::process(bytes, true, true);
        else {
            auto warmup = Engine::process(bytes, mode == "book", false);
            auto start = std::chrono::steady_clock::now();
            result = Engine::process(bytes, mode == "book", false);
            seconds =
                std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
            if (result.messages != warmup.messages || result.sink != warmup.sink)
                throw std::runtime_error("nondeterministic timed result");
        }
        std::sort(result.levels.begin(), result.levels.end());
        std::cout << std::setprecision(17) << "{\"engine\":\"" << Engine::name << "\",\"mode\":\""
                  << mode << "\",\"messages\":" << result.messages << ",\"sink\":" << result.sink
                  << ",\"decoded_digest\":" << result.digest.value << ",\"seconds\":" << seconds
                  << ",\"peak_rss_bytes\":" << peak_rss() << ",\"levels\":[";
        bool first = true;
        for (const auto &l : result.levels) {
            if (!first)
                std::cout << ',';
            first = false;
            std::cout << '[' << l[0] << ',' << l[1] << ',' << l[2] << ',' << l[3] << ',' << l[4]
                      << ']';
        }
        std::cout << "]}\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
} // namespace comparison
