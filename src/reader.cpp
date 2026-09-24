#include "itch/reader.hpp"
namespace itch {
bool Reader::next(Frame &f) {
    const auto at = offset_;
    char prefix[2];
    input_.read(prefix, 2);
    auto n = input_.gcount();
    offset_ += static_cast<std::uint64_t>(n);
    if (n == 0 && input_.eof())
        return false;
    if (n != 2)
        throw Error(at, 0, "truncated length prefix or read failure");
    auto length =
        (static_cast<unsigned char>(prefix[0]) << 8) | static_cast<unsigned char>(prefix[1]);
    if (length == 0)
        throw Error(at, 0, "zero length message");
    f.offset = at;
    f.bytes.resize(static_cast<std::size_t>(length));
    input_.read(reinterpret_cast<char *>(f.bytes.data()), length);
    n = input_.gcount();
    offset_ += static_cast<std::uint64_t>(n);
    if (n != length)
        throw Error(at, n > 0 ? static_cast<char>(f.bytes[0]) : 0,
                    "truncated payload or read failure");
    return true;
}
} // namespace itch
