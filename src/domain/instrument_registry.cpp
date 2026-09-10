#include "exchange_core/domain/instrument_registry.hpp"

namespace exchange_core::domain
{
    bool InstrumentRegistry::add(Instrument instrument)
    {
        if (instrument.instrument_id == 0 || instrument.symbol.empty() ||
            !instrument.accepts_orders())
        {
            return false;
        }

        return instruments_.emplace(instrument.instrument_id, std::move(instrument)).second;
    }

    const Instrument *InstrumentRegistry::find(InstrumentId instrument_id) const
    {
        const auto instrument = instruments_.find(instrument_id);
        if (instrument == instruments_.end())
        {
            return nullptr;
        }
        return &instrument->second;
    }

    bool InstrumentRegistry::contains(InstrumentId instrument_id) const
    {
        return instruments_.find(instrument_id) != instruments_.end();
    }

    std::vector<Instrument> InstrumentRegistry::snapshot() const
    {
        std::vector<Instrument> instruments;
        instruments.reserve(instruments_.size());
        for (const auto &entry : instruments_)
        {
            instruments.push_back(entry.second);
        }
        return instruments;
    }
}
