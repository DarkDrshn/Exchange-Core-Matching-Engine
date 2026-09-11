#pragma once

#include "exchange_core/api/order_types.hpp"
#include "exchange_core/domain/order.hpp"

#include <string>
#include <variant>

namespace exchange_core::api
{

    enum class RejectReason
    {
        invalid_order,
        duplicate_order_id,
        unknown_order_id,
        unknown_instrument,
    };

    struct OrderAccepted
    {
        domain::Order order{};
        domain::InstrumentId instrument_id{};
        OrderId order_id{};
    };

    struct TradeExecuted
    {
        domain::Trade trade{};
        domain::InstrumentId instrument_id{};
        OrderId incoming_order_id{};
        OrderId resting_order_id{};
        Price execution_price{};
        Quantity execution_quantity{};
    };

    struct OrderCanceled
    {
        domain::Order order{};
        domain::InstrumentId instrument_id{};
        OrderId order_id{};
    };

    struct OrderRejected
    {
        domain::Order order{};
        domain::InstrumentId instrument_id{};
        OrderId order_id{};
        RejectReason reason{};
    };

    using EngineEvent = std::variant<
        OrderAccepted,
        TradeExecuted,
        OrderCanceled,
        OrderRejected>;

} // namespace exchange_core::api
