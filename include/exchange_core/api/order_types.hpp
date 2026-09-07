#pragma once

#include <cstdint>

namespace exchange_core::api
{
    enum class Side
    {
        buy,
        sell,
    };

    using OrderId = std::uint64_t;
    using Price = std::int64_t;
    using Quantity = std::uint64_t;

    struct PlaceOrder
    {
        OrderId order_id{};
        Side side{};
        Price price{};
        Quantity quantity{};
    };

    struct CancelOrder
    {
        OrderId order_id{};
    };

} // namespace exchange_core::api
