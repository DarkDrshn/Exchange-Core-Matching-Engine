#pragma once

#include "exchange_core/api/order_types.hpp"
#include "exchange_core/domain/value_types.hpp"

#include <cstdint>

namespace exchange_core::domain
{
    using ExecutionId = std::uint64_t;

    enum class OrderStatus
    {
        new_order,
        partially_filled,
        filled,
        canceled,
        rejected,
    };

    struct Order
    {
        InstrumentId instrument_id{};
        api::OrderId order_id{};
        api::Side side{};
        Price price{0};
        Quantity quantity{0};
        Quantity remaining_quantity{0};
        OrderStatus status{OrderStatus::new_order};
        api::OrderType order_type{api::OrderType::limit};
        api::AccountId account_id{};
    };

    struct Trade
    {
        ExecutionId execution_id{};
        InstrumentId instrument_id{};
        api::OrderId incoming_order_id{};
        api::OrderId resting_order_id{};
        Price execution_price{0};
        Quantity execution_quantity{0};
        api::AccountId incoming_account_id{};
        api::AccountId resting_account_id{};
    };
}
