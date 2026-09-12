#include "exchange_core/risk/risk_manager.hpp"

namespace exchange_core::risk
{
    RejectReason RiskManager::evaluate(const api::PlaceOrder &request) const
    {
        if (request.quantity > configuration_.maximum_order_quantity)
        {
            return RejectReason::quantity_limit;
        }

        if (request.order_type == api::OrderType::market)
        {
            return RejectReason::none;
        }

        if (request.price > configuration_.maximum_order_price)
        {
            return RejectReason::fat_finger_limit;
        }

        const auto price = static_cast<api::Quantity>(request.price);
        if (request.quantity > configuration_.maximum_order_notional / price)
        {
            return RejectReason::notional_limit;
        }

        return RejectReason::none;
    }

} // namespace exchange_core::risk
