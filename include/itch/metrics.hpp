#pragma once
#include <cstdint>
#include <map>
#include <ostream>
#include <string>
namespace itch {
struct Metrics {
    std::uint64_t messages{}, bytes{}, unsupported{}, filtered{}, errors{};
    double elapsed_seconds{};
    std::map<char, std::uint64_t> by_type;
    void write_json(std::ostream &output, const std::string &input, const std::string &error,
                    std::size_t active_orders) const;
};
std::string json_string(const std::string &value);
} // namespace itch
