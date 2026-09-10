#pragma once

#include "exchange_core/domain/instrument.hpp"

#include <unordered_map>
#include <vector>

namespace exchange_core::domain
{
    class InstrumentRegistry
    {
    public:
        bool add(Instrument instrument);
        [[nodiscard]] const Instrument *find(InstrumentId instrument_id) const;
        [[nodiscard]] bool contains(InstrumentId instrument_id) const;
        [[nodiscard]] std::vector<Instrument> snapshot() const;

    private:
        std::unordered_map<InstrumentId, Instrument> instruments_;
    };
}
