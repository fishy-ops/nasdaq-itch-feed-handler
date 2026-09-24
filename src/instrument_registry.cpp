#include "itch/instrument_registry.hpp"
namespace itch {
void InstrumentRegistry::rebuild_index() {
    by_locate_.clear();
    if (!instruments_.empty())
        by_locate_.resize(static_cast<std::size_t>(instruments_.rbegin()->first) + 1);
    for (const auto &[loc, instrument] : instruments_)
        by_locate_[loc] = &instrument;
}
InstrumentRegistry::InstrumentRegistry(const InstrumentRegistry &other)
    : instruments_(other.instruments_), session_(other.session_) {
    rebuild_index();
}
InstrumentRegistry &InstrumentRegistry::operator=(const InstrumentRegistry &other) {
    if (this != &other) {
        InstrumentRegistry copy(other);
        instruments_.swap(copy.instruments_);
        by_locate_.swap(copy.by_locate_);
        std::swap(session_, copy.session_);
    }
    return *this;
}
const Instrument &InstrumentRegistry::at(std::uint16_t locate) const {
    if (locate >= by_locate_.size() || !by_locate_[locate])
        throw std::runtime_error("missing stock directory");
    return *by_locate_[locate];
}
void InstrumentRegistry::apply(const Event &e) {
    if (auto d = std::get_if<Directory>(&e.payload)) {
        if (!e.header.locate || d->symbol.empty())
            throw std::runtime_error("invalid directory identity");
        if (instruments_.contains(e.header.locate))
            throw std::runtime_error("duplicate stock locate");
        for (const auto &[loc, i] : instruments_) {
            (void)loc;
            if (i.directory.symbol == d->symbol)
                throw std::runtime_error("duplicate stock symbol");
        }
        if (e.header.locate >= by_locate_.size())
            by_locate_.resize(static_cast<std::size_t>(e.header.locate) + 1);
        auto [position, inserted] = instruments_.emplace(e.header.locate, Instrument{*d, '?', {}});
        (void)inserted;
        by_locate_[e.header.locate] = &position->second;
    } else if (auto s = std::get_if<System>(&e.payload)) {
        if (e.header.locate != 0)
            throw std::runtime_error("system event has nonzero locate");
        if (s->code == 'O' && session_ != '?')
            throw std::runtime_error("multiple sessions are unsupported");
        session_ = s->code;
    } else if (auto h = std::get_if<TradingAction>(&e.payload)) {
        if (at(e.header.locate).directory.symbol != h->symbol)
            throw std::runtime_error("trading action symbol mismatch");
        auto &i = instruments_.at(e.header.locate);
        i.state = h->state;
        i.reason = h->reason;
    }
}
} // namespace itch
