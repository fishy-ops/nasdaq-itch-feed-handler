#pragma once
#include "itch/events.hpp"
#include <span>
namespace itch {
Event decode(std::span<const std::uint8_t> bytes, std::uint64_t offset = 0);
}
