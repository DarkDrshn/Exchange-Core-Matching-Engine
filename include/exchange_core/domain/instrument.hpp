#pragma once

#include "exchange_core/domain/value_types.hpp"

#include <cstdint>
#include <string>

namespace exchange_core::domain
{
    using InstrumentId = std::uint32_t;

    enum class InstrumentStatus
    {
        active,
        halted,
    };

    struct Instrument
    {
        InstrumentId instrument_id{};
        std::string symbol;
        Price tick_size{1};
        Quantity lot_size{1};
        InstrumentStatus status{InstrumentStatus::active};

        [[nodiscard]] bool accepts_orders() const
        {
            return status == InstrumentStatus::active &&
                   tick_size.is_positive() && lot_size.is_positive();
        }
    };
}
