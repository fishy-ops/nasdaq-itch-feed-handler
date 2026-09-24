#pragma once
#include "itch/events.hpp"
#include <map>
#include <vector>
namespace itch {
struct Instrument {
    Directory directory;
    char state{'?'};
    std::string reason;
};
class InstrumentRegistry {
    std::map<std::uint16_t, Instrument> instruments_;
    char session_{'?'};
    // Ordered ownership for stable output; locate-indexed links for hot reads.
    std::vector<const Instrument *> by_locate_;
    void rebuild_index();

  public:
    InstrumentRegistry() = default;
    InstrumentRegistry(const InstrumentRegistry &other);
    InstrumentRegistry &operator=(const InstrumentRegistry &other);
    InstrumentRegistry(InstrumentRegistry &&) noexcept = default;
    InstrumentRegistry &operator=(InstrumentRegistry &&) noexcept = default;
    void apply(const Event &event);
    const Instrument &at(std::uint16_t locate) const;
    const auto &all() const {
        return instruments_;
    }
    char session() const {
        return session_;
    }
};
} // namespace itch
