#pragma once

#include "exchange_core/api/events.hpp"

#include <deque>
#include <functional>
#include <map>
#include <unordered_map>
#include <vector>

namespace exchange_core::domain
{

    class OrderBook
    {
    public:
        using EventBatch = std::vector<api::EngineEvent>;

        EventBatch place_order(const api::PlaceOrder &request);
        EventBatch cancel_order(const api::CancelOrder &request);
        // cppcheck-suppress syntaxError
        [[nodiscard]] bool contains_order(api::OrderId order_id) const;

    private:
        struct RestingOrder
        {
            api::OrderId order_id{};
            api::Quantity remaining_quantity{};
        };

        struct OrderLocation
        {
            api::Side side{};
            api::Price price{};
        };

        using BuyLevels = std::map<api::Price, std::deque<RestingOrder>, std::greater<>>;
        using SellLevels = std::map<api::Price, std::deque<RestingOrder>>;

        EventBatch reject(api::OrderId order_id, api::RejectReason reason) const;
        void remove_empty_level(api::Side side, api::Price price);
        void remove_order_from_level(api::OrderId order_id, const OrderLocation &location);

        BuyLevels buy_levels_;
        SellLevels sell_levels_;
        std::unordered_map<api::OrderId, OrderLocation> order_locations_;
    };

} // namespace exchange_core::domain
