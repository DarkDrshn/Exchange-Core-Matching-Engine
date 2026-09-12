#pragma once

#include "exchange_core/api/order_types.hpp"

#include <limits>

namespace exchange_core::engine
{
    struct EngineConfig
    {
        api::Price maximum_order_price{std::numeric_limits<api::Price>::max()};
        api::Quantity maximum_order_quantity{std::numeric_limits<api::Quantity>::max()};
        api::Quantity maximum_order_notional{std::numeric_limits<api::Quantity>::max()};
        api::Quantity maximum_position_quantity{std::numeric_limits<api::Quantity>::max()};
        api::Quantity maximum_open_order_quantity{std::numeric_limits<api::Quantity>::max()};
        api::Quantity maximum_account_credit{std::numeric_limits<api::Quantity>::max()};
        api::Quantity initial_margin_basis_points{10000};

        [[nodiscard]] constexpr bool is_valid() const
        {
                 return maximum_order_price > 0 && maximum_order_quantity > 0 &&
                       maximum_order_notional > 0 && maximum_account_credit > 0 &&
                       initial_margin_basis_points <= 10000;
        }
    };

    static_assert(EngineConfig{}.is_valid());
}
