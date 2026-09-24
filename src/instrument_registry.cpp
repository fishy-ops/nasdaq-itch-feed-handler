#include "itch/instrument_registry.hpp"
namespace itch {
const Instrument &InstrumentRegistry::at(std::uint16_t locate) const {
    auto it = instruments_.find(locate);
    if (it == instruments_.end())
        throw std::runtime_error("missing stock directory");
    return it->second;
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
        instruments_.emplace(e.header.locate, Instrument{*d, '?', {}});
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
