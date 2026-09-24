#include "fixture.hpp"
#include <charconv>
#include <fstream>
#include <iostream>
int main(int argc, char **argv) {
    try {
        if (argc < 2 || argc > 3)
            throw std::runtime_error("usage: generate_fixture OUTPUT [CYCLES]");
        std::uint64_t cycles = 0;
        if (argc == 3) {
            std::string s = argv[2];
            auto [p, ec] = std::from_chars(s.data(), s.data() + s.size(), cycles);
            if (ec != std::errc{} || p != s.data() + s.size() || cycles > 1000000000ULL)
                throw std::runtime_error("cycles must be 0..1000000000");
        }
        std::ofstream out;
        out.exceptions(std::ios::badbit | std::ios::failbit);
        out.open(argv[1], std::ios::binary);
        if (!cycles) {
            for (const auto &b : fixture::demo())
                fixture::write(out, b);
        } else {
            fixture::write(out, fixture::system('O'));
            fixture::write(out, fixture::directory(1, "AAPL"));
            fixture::write(out, fixture::trading(1, "AAPL"));
            for (std::uint64_t i = 0; i < cycles; ++i) {
                auto id = 1 + 2 * i;
                fixture::write(out, fixture::add(id, 'B', 100, 1500000));
                fixture::write(out, fixture::reduce('E', id, 20));
                fixture::write(out, fixture::replace(id, id + 1, 70, 1500100));
                fixture::write(out, fixture::erase(id + 1));
            }
            fixture::write(out, fixture::system('C'));
        }
        out.close();
        return 0;
    } catch (const std::exception &e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
