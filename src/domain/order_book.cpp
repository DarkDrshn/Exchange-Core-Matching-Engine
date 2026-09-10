#include "domain/order_book.hpp"

#include <algorithm>

namespace exchange_core::domain
{

    OrderBook::EventBatch OrderBook::reject(
        InstrumentId instrument_id, api::OrderId order_id, api::RejectReason reason) const
    {
        return {api::OrderRejected{instrument_id, order_id, reason}};
    }

    OrderBook::EventBatch OrderBook::place_order(const Order &order)
    {
        if (order_locations_.find(order.order_id) != order_locations_.end())
        {
            return reject(order.instrument_id, order.order_id, api::RejectReason::duplicate_order_id);
        }

        EventBatch events;
        events.emplace_back(api::OrderAccepted{order.instrument_id, order.order_id});
        Quantity remaining_quantity = order.quantity;

        if (order.side == api::Side::buy)
        {
            while (remaining_quantity > Quantity{0} && !sell_levels_.empty())
            {
                auto best_level = sell_levels_.begin();
                if (order.price < best_level->first)
                {
                    break;
                }

                auto &resting_orders = best_level->second;
                while (remaining_quantity > Quantity{0} && !resting_orders.empty())
                {
                    auto &resting_order = resting_orders.front();
                    const Quantity executed_quantity =
                        std::min(remaining_quantity, resting_order.remaining_quantity);
                    events.emplace_back(api::TradeExecuted{
                        order.instrument_id,
                        order.order_id,
                        resting_order.order_id,
                        best_level->first.value(),
                        executed_quantity.value()});
                    remaining_quantity -= executed_quantity;
                    resting_order.remaining_quantity -= executed_quantity;

                    if (resting_order.remaining_quantity == Quantity{0})
                    {
                        order_locations_.erase(resting_order.order_id);
                        resting_orders.pop_front();
                    }
                }
                remove_empty_level(api::Side::sell, best_level->first);
            }
        }
        else
        {
            while (remaining_quantity > Quantity{0} && !buy_levels_.empty())
            {
                auto best_level = buy_levels_.begin();
                if (order.price > best_level->first)
                {
                    break;
                }

                auto &resting_orders = best_level->second;
                while (remaining_quantity > Quantity{0} && !resting_orders.empty())
                {
                    auto &resting_order = resting_orders.front();
                    const Quantity executed_quantity =
                        std::min(remaining_quantity, resting_order.remaining_quantity);
                    events.emplace_back(api::TradeExecuted{
                        order.instrument_id,
                        order.order_id,
                        resting_order.order_id,
                        best_level->first.value(),
                        executed_quantity.value()});
                    remaining_quantity -= executed_quantity;
                    resting_order.remaining_quantity -= executed_quantity;

                    if (resting_order.remaining_quantity == Quantity{0})
                    {
                        order_locations_.erase(resting_order.order_id);
                        resting_orders.pop_front();
                    }
                }
                remove_empty_level(api::Side::buy, best_level->first);
            }
        }

        if (remaining_quantity > Quantity{0})
        {
            const OrderLocation location{order.side, order.price};
            order_locations_.emplace(order.order_id, location);
            if (order.side == api::Side::buy)
            {
                buy_levels_[order.price].push_back(
                    RestingOrder{order.order_id, remaining_quantity});
            }
            else
            {
                sell_levels_[order.price].push_back(
                    RestingOrder{order.order_id, remaining_quantity});
            }
        }

        return events;
    }

    OrderBook::EventBatch OrderBook::cancel_order(const api::CancelOrder &request)
    {
        const auto location = order_locations_.find(request.order_id);
        if (location == order_locations_.end())
        {
            return reject(request.instrument_id, request.order_id, api::RejectReason::unknown_order_id);
        }

        remove_order_from_level(request.order_id, location->second);
        order_locations_.erase(location);
        return {api::OrderCanceled{request.instrument_id, request.order_id}};
    }

    bool OrderBook::contains_order(api::OrderId order_id) const
    {
        return order_locations_.find(order_id) != order_locations_.end();
    }

    void OrderBook::remove_empty_level(api::Side side, Price price)
    {
        if (side == api::Side::buy)
        {
            auto level = buy_levels_.find(price);
            if (level != buy_levels_.end() && level->second.empty())
            {
                buy_levels_.erase(level);
            }
            return;
        }

        auto level = sell_levels_.find(price);
        if (level != sell_levels_.end() && level->second.empty())
        {
            sell_levels_.erase(level);
        }
    }

    void OrderBook::remove_order_from_level(
        api::OrderId order_id,
        const OrderLocation &location)
    {
        if (location.side == api::Side::buy)
        {
            auto level = buy_levels_.find(location.price);
            if (level == buy_levels_.end())
            {
                return;
            }
            auto &orders = level->second;
            for (auto order = orders.begin(); order != orders.end(); ++order)
            {
                if (order->order_id == order_id)
                {
                    orders.erase(order);
                    break;
                }
            }
            remove_empty_level(api::Side::buy, location.price);
            return;
        }

        auto level = sell_levels_.find(location.price);
        if (level == sell_levels_.end())
        {
            return;
        }
        auto &orders = level->second;
        for (auto order = orders.begin(); order != orders.end(); ++order)
        {
            if (order->order_id == order_id)
            {
                orders.erase(order);
                break;
            }
        }
        remove_empty_level(api::Side::sell, location.price);
    }

} // namespace exchange_core::domain
