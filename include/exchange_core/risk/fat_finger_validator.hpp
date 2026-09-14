#pragma once

#include "exchange_core/api/order_types.hpp"
#include "exchange_core/engine/engine_config.hpp"

namespace exchange_core::risk
{
    enum class FatFingerResult
    {
        accepted,
        reference_price_unavailable,
        outside_reference_band,
    };

    class FatFingerValidator
    {
    public:
        explicit FatFingerValidator(engine::EngineConfig configuration)
            : configuration_(configuration)
        {
        }

        [[nodiscard]] FatFingerResult evaluate(
            const api::PlaceOrder &request, api::Price reference_price) const;

    private:
        engine::EngineConfig configuration_;
    };

} // namespace exchange_core::risk