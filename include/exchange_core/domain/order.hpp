#pragma once

#include "exchange_core/api/order_types.hpp"
#include "exchange_core/domain/value_types.hpp"

namespace exchange_core::domain
{
    struct Order
    {
        api::OrderId order_id{};
        api::Side side{};
        Price price{0};
        Quantity quantity{0};
    };
}
