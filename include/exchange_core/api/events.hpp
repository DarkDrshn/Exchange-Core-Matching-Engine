#pragma once

#include "exchange_core/api/order_types.hpp"

#include <string>
#include <variant>

namespace exchange_core::api
{

    enum class RejectReason
    {
        invalid_order,
        duplicate_order_id,
        unknown_order_id,
    };

    struct OrderAccepted
    {
        OrderId order_id{};
    };

    struct TradeExecuted
    {
        OrderId incoming_order_id{};
        OrderId resting_order_id{};
        Price execution_price{};
        Quantity execution_quantity{};
    };

    struct OrderCanceled
    {
        OrderId order_id{};
    };

    struct OrderRejected
    {
        OrderId order_id{};
        RejectReason reason{};
    };

    using EngineEvent = std::variant<
        OrderAccepted,
        TradeExecuted,
        OrderCanceled,
        OrderRejected>;

} // namespace exchange_core::api
