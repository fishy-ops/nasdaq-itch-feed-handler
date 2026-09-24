#include "itch/decoder.hpp"
namespace itch {
Event decode(std::span<const std::uint8_t> b, std::uint64_t offset) {
    const char type = b.empty() ? 0 : static_cast<char>(b[0]);
    auto fail = [&](const char *reason) { throw Error(offset, type, reason); };
    if (b.size() < 11)
        fail("message shorter than common header");
    std::size_t expected = 0;
    switch (type) {
    case 'S':
        expected = 12;
        break;
    case 'R':
        expected = 39;
        break;
    case 'H':
        expected = 25;
        break;
    case 'A':
        expected = 36;
        break;
    case 'F':
        expected = 40;
        break;
    case 'E':
        expected = 31;
        break;
    case 'C':
        expected = 36;
        break;
    case 'X':
        expected = 23;
        break;
    case 'D':
        expected = 19;
        break;
    case 'U':
        expected = 35;
        break;
    // Validate framing for known, intentionally unsupported messages too.
    case 'Y':
        expected = 20;
        break;
    case 'L':
        expected = 26;
        break;
    case 'V':
        expected = 35;
        break;
    case 'W':
        expected = 12;
        break;
    case 'K':
        expected = 28;
        break;
    case 'J':
        expected = 35;
        break;
    case 'h':
        expected = 21;
        break;
    case 'P':
        expected = 44;
        break;
    case 'Q':
        expected = 40;
        break;
    case 'B':
        expected = 19;
        break;
    case 'I':
        expected = 50;
        break;
    case 'N':
        expected = 20;
        break;
    case 'O':
        expected = 48;
        break;
    default:
        break;
    }
    if (expected && b.size() != expected)
        fail("incorrect message length");
    auto number = [&](std::size_t pos, std::size_t count) {
        std::uint64_t v = 0;
        for (std::size_t i = 0; i < count; ++i)
            v = (v << 8) | b[pos + i];
        return v;
    };
    auto u32 = [&](std::size_t pos) { return static_cast<std::uint32_t>(number(pos, 4)); };
    auto str = [&](std::size_t pos, std::size_t count) {
        std::string s;
        for (std::size_t i = 0; i < count; ++i) {
            auto c = b[pos + i];
            if (c < 32 || c > 126)
                fail("non-ASCII text field");
            s += static_cast<char>(c);
        }
        while (!s.empty() && s.back() == ' ')
            s.pop_back();
        return s;
    };
    Event e{{type, static_cast<std::uint16_t>(number(1, 2)),
             static_cast<std::uint16_t>(number(3, 2)), number(5, 6)},
            Unsupported{}};
    if (e.header.timestamp >= 86400000000000ULL)
        fail("timestamp outside day");
    switch (type) {
    case 'S':
        if (std::string("OSQMEC").find(static_cast<char>(b[11])) == std::string::npos)
            fail("invalid system event");
        e.payload = System{static_cast<char>(b[11])};
        break;
    case 'R':
        e.payload =
            Directory{str(11, 8), static_cast<char>(b[19]), static_cast<char>(b[20]), u32(21)};
        break;
    case 'H':
        if (std::string("HPTQ").find(static_cast<char>(b[19])) == std::string::npos)
            fail("invalid trading state");
        e.payload = TradingAction{str(11, 8), static_cast<char>(b[19]), str(21, 4)};
        break;
    case 'A':
    case 'F':
        if (b[19] != 'B' && b[19] != 'S')
            fail("invalid order side");
        e.payload = Add{number(11, 8), static_cast<char>(b[19]),     u32(20), u32(32),
                        str(24, 8),    type == 'F' ? str(36, 4) : ""};
        break;
    case 'E':
    case 'C':
    case 'X':
        if (type == 'C' && b[31] != 'Y' && b[31] != 'N')
            fail("invalid printable flag");
        e.payload =
            Reduce{number(11, 8), u32(19), type == 'X' ? 0 : number(23, 8),
                   type == 'C' ? static_cast<char>(b[31]) : char{}, type == 'C' ? u32(32) : 0};
        break;
    case 'D':
        e.payload = Delete{number(11, 8)};
        break;
    case 'U':
        e.payload = Replace{number(11, 8), number(19, 8), u32(27), u32(31)};
        break;
    default:
        break;
    }
    return e;
}
} // namespace itch
