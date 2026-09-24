#pragma once
#include <cstdint>
#include <stdexcept>
#include <string>
#include <variant>
namespace itch {
struct Header {
    char type{};
    std::uint16_t locate{}, tracking{};
    std::uint64_t timestamp{};
};
struct Directory {
    std::string symbol;
    char market{}, financial{};
    std::uint32_t round_lot{};
};
struct System {
    char code{};
};
struct TradingAction {
    std::string symbol;
    char state{};
    std::string reason;
};
struct Add {
    std::uint64_t id{};
    char side{};
    std::uint32_t quantity{}, price{};
    std::string symbol, participant;
};
struct Reduce {
    std::uint64_t id{};
    std::uint32_t quantity{};
    std::uint64_t match{};
    char printable{};
    std::uint32_t execution_price{};
};
struct Delete {
    std::uint64_t id{};
};
struct Replace {
    std::uint64_t old_id{}, new_id{};
    std::uint32_t quantity{}, price{};
};
struct Unsupported {};
using Payload =
    std::variant<Directory, System, TradingAction, Add, Reduce, Delete, Replace, Unsupported>;
struct Event {
    Header header;
    Payload payload;
};
struct Error : std::runtime_error {
    std::uint64_t offset;
    char type;
    Error(std::uint64_t at, char t, const std::string &reason)
        : std::runtime_error("offset=" + std::to_string(at) +
                             " type=" + (t ? std::string(1, t) : "?") + ": " + reason),
          offset(at), type(t) {}
};
} // namespace itch
