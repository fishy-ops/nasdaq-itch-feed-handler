#pragma once
#include "itch/events.hpp"
#include <istream>
#include <span>
#include <vector>
namespace itch {
struct Frame {
    std::uint64_t offset{};
    std::vector<std::uint8_t> bytes;
};
class Reader {
    std::istream &input_;
    std::uint64_t offset_{};

  public:
    explicit Reader(std::istream &input) : input_(input) {}
    bool next(Frame &frame);
    std::uint64_t bytes_read() const {
        return offset_;
    }
};
} // namespace itch
