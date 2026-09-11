#pragma once

#include "exchange_core/api/events.hpp"
#include "exchange_core/domain/order.hpp"

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

        EventBatch place_order(const Order &order);
        EventBatch cancel_order(const api::CancelOrder &request);
        // cppcheck-suppress syntaxError
        [[nodiscard]] bool contains_order(api::OrderId order_id) const;

    private:
        struct RestingOrder
        {
            api::OrderId order_id{};
            Quantity remaining_quantity{0};
            api::OrderType order_type{api::OrderType::limit};
        };

        struct OrderLocation
        {
            api::Side side{};
            Price price{0};
        };

        using BuyLevels = std::map<Price, std::deque<RestingOrder>, std::greater<>>;
        using SellLevels = std::map<Price, std::deque<RestingOrder>>;

        EventBatch reject(const Order &order, api::RejectReason reason) const;
        [[nodiscard]] bool can_fully_match(const Order &order) const;
        void remove_empty_level(api::Side side, Price price);
        void remove_order_from_level(api::OrderId order_id, const OrderLocation &location);

        BuyLevels buy_levels_;
        SellLevels sell_levels_;
        std::unordered_map<api::OrderId, OrderLocation> order_locations_;
        std::uint64_t next_execution_id_{1};
    };

} // namespace exchange_core::domain
