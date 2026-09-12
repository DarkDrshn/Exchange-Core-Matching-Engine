#pragma once

#include "exchange_core/api/order_types.hpp"
#include "exchange_core/engine/engine_config.hpp"

namespace exchange_core::risk
{
    enum class RejectReason
    {
        none,
        quantity_limit,
        notional_limit,
        fat_finger_limit,
    };

    class RiskManager
    {
    public:
        explicit RiskManager(const engine::EngineConfig &configuration)
            : configuration_(configuration)
        {
        }

        [[nodiscard]] RejectReason evaluate(const api::PlaceOrder &request) const;

    private:
        engine::EngineConfig configuration_;
    };

} // namespace exchange_core::risk
