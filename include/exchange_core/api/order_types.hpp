#pragma once

#include <cstdint>

#include "exchange_core/domain/instrument.hpp"

namespace exchange_core::api
{
    enum class Side
    {
        buy,
        sell,
    };

    enum class OrderType
    {
        limit,
        market,
        ioc,
        fok,
        post_only,
    };

    using OrderId = std::uint64_t;
    using AccountId = std::uint64_t;
    using Price = std::int64_t;
    using Quantity = std::uint64_t;

    struct PlaceOrder
    {
        domain::InstrumentId instrument_id{};
        OrderId order_id{};
        Side side{};
        Price price{};
        Quantity quantity{};
        OrderType order_type{OrderType::limit};
        AccountId account_id{};
    };

    struct CancelOrder
    {
        domain::InstrumentId instrument_id{};
        OrderId order_id{};
        AccountId account_id{};
    };

} // namespace exchange_core::api
