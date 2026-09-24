#pragma once
#include "itch/events.hpp"
#include <map>
namespace itch {
struct Instrument {
    Directory directory;
    char state{'?'};
    std::string reason;
};
class InstrumentRegistry {
    std::map<std::uint16_t, Instrument> instruments_;
    char session_{'?'};

  public:
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
