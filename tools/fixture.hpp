#pragma once
#include <cstdint>
#include <ostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace fixture {
using Bytes = std::vector<std::uint8_t>;
inline void number(Bytes &b, std::size_t pos, std::size_t width, std::uint64_t value) {
    for (std::size_t i = 0; i < width; ++i) {
        b.at(pos + width - 1 - i) = static_cast<std::uint8_t>(value & 255);
        value >>= 8;
    }
}
inline void text(Bytes &b, std::size_t pos, std::size_t width, const std::string &s) {
    for (std::size_t i = 0; i < width; ++i)
        b.at(pos + i) = static_cast<std::uint8_t>(i < s.size() ? s[i] : ' ');
}
inline Bytes header(char type, std::size_t size, std::uint16_t locate = 1) {
    Bytes b(size);
    b[0] = static_cast<std::uint8_t>(type);
    number(b, 1, 2, locate);
    number(b, 3, 2, 17);
    number(b, 5, 6, 34200000000000ULL);
    return b;
}
inline Bytes system(char code) {
    auto b = header('S', 12, 0);
    b[11] = static_cast<std::uint8_t>(code);
    return b;
}
inline Bytes directory(std::uint16_t locate, const std::string &symbol) {
    auto b = header('R', 39, locate);
    text(b, 11, 8, symbol);
    b[19] = 'Q';
    b[20] = 'N';
    number(b, 21, 4, 100);
    b[25] = 'N';
    b[26] = 'C';
    text(b, 27, 2, " ");
    b[29] = 'T';
    b[30] = 'N';
    b[31] = 'N';
    b[32] = '1';
    b[33] = 'N';
    b[38] = 'N';
    return b;
}
inline Bytes trading(std::uint16_t locate, const std::string &symbol, char state = 'T') {
    auto b = header('H', 25, locate);
    text(b, 11, 8, symbol);
    b[19] = static_cast<std::uint8_t>(state);
    b[20] = ' ';
    text(b, 21, 4, "");
    return b;
}
inline Bytes add(std::uint64_t id, char side, std::uint32_t qty, std::uint32_t price,
                 std::uint16_t locate = 1, const std::string &symbol = "AAPL",
                 bool attributed = false) {
    auto b = header(attributed ? 'F' : 'A', attributed ? 40 : 36, locate);
    number(b, 11, 8, id);
    b[19] = static_cast<std::uint8_t>(side);
    number(b, 20, 4, qty);
    text(b, 24, 8, symbol);
    number(b, 32, 4, price);
    if (attributed)
        text(b, 36, 4, "TEST");
    return b;
}
inline Bytes reduce(char type, std::uint64_t id, std::uint32_t qty, std::uint16_t locate = 1) {
    auto b = header(type, type == 'X' ? 23 : type == 'E' ? 31 : 36, locate);
    number(b, 11, 8, id);
    number(b, 19, 4, qty);
    if (type != 'X')
        number(b, 23, 8, 999);
    if (type == 'C') {
        b[31] = 'Y';
        number(b, 32, 4, 1500200);
    }
    return b;
}
inline Bytes erase(std::uint64_t id, std::uint16_t locate = 1) {
    auto b = header('D', 19, locate);
    number(b, 11, 8, id);
    return b;
}
inline Bytes replace(std::uint64_t old_id, std::uint64_t new_id, std::uint32_t qty,
                     std::uint32_t price, std::uint16_t locate = 1) {
    auto b = header('U', 35, locate);
    number(b, 11, 8, old_id);
    number(b, 19, 8, new_id);
    number(b, 27, 4, qty);
    number(b, 31, 4, price);
    return b;
}
inline std::vector<Bytes> demo() {
    return {system('O'),
            directory(1, "AAPL"),
            directory(2, "MSFT"),
            trading(1, "AAPL"),
            trading(2, "MSFT"),
            add(100, 'B', 100, 1500000),
            add(101, 'B', 50, 1500000, 1, "AAPL", true),
            add(102, 'S', 120, 1500500),
            add(200, 'B', 80, 3000000, 2, "MSFT"),
            reduce('E', 100, 20),
            reduce('C', 102, 10),
            reduce('X', 101, 10),
            replace(100, 103, 70, 1500100),
            erase(101),
            header('P', 44),
            system('C')};
}
inline void write(std::ostream &out, const Bytes &b) {
    if (b.empty() || b.size() > 65535)
        throw std::runtime_error("invalid fixture length");
    char prefix[2] = {static_cast<char>(b.size() >> 8), static_cast<char>(b.size() & 255)};
    out.write(prefix, 2);
    out.write(reinterpret_cast<const char *>(b.data()), static_cast<std::streamsize>(b.size()));
}
} // namespace fixture
