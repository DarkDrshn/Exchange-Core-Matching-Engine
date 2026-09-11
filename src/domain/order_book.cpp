#include "domain/order_book.hpp"

#include <algorithm>

namespace exchange_core::domain
{
    OrderBook::EventBatch OrderBook::reject(const Order &order, api::RejectReason reason) const
    {
        const domain::Order rejected_order{
            order.instrument_id,
            order.order_id,
            order.side,
            order.price,
            order.quantity,
            order.quantity,
            domain::OrderStatus::rejected,
            order.order_type};

        return {api::OrderRejected{rejected_order, order.instrument_id, order.order_id, reason}};
    }

    OrderBook::EventBatch OrderBook::place_order(const Order &order)
    {
        if (order_locations_.find(order.order_id) != order_locations_.end())
        {
            return reject(order, api::RejectReason::duplicate_order_id);
        }

        if (order.order_type == api::OrderType::fok && !can_fully_match(order))
        {
            return reject(order, api::RejectReason::fok_not_filled);
        }

        if (order.order_type == api::OrderType::post_only)
        {
            const bool crosses_book = (order.side == api::Side::buy && !sell_levels_.empty() &&
                                          order.price >= sell_levels_.begin()->first) ||
                                     (order.side == api::Side::sell && !buy_levels_.empty() &&
                                          order.price <= buy_levels_.begin()->first);
            if (crosses_book)
            {
                return reject(order, api::RejectReason::post_only_rejected);
            }
        }

        EventBatch events;
        domain::Order accepted_order = order;
        accepted_order.remaining_quantity = order.quantity;
        accepted_order.status = domain::OrderStatus::new_order;
        events.emplace_back(api::OrderAccepted{accepted_order, order.instrument_id, order.order_id});
        Quantity remaining_quantity = order.quantity;

        if (order.side == api::Side::buy)
        {
            while (remaining_quantity > Quantity{0} && !sell_levels_.empty())
            {
                auto best_level = sell_levels_.begin();
                if (order.order_type != api::OrderType::market &&
                    order.price < best_level->first)
                {
                    break;
                }

                auto &resting_orders = best_level->second;
                while (remaining_quantity > Quantity{0} && !resting_orders.empty())
                {
                    auto &resting_order = resting_orders.front();
                    const Quantity executed_quantity =
                        std::min(remaining_quantity, resting_order.remaining_quantity);
                    const domain::Trade trade{
                        next_execution_id_++,
                        order.instrument_id,
                        order.order_id,
                        resting_order.order_id,
                        best_level->first,
                        executed_quantity};
                    events.emplace_back(api::TradeExecuted{
                        trade,
                        order.instrument_id,
                        order.order_id,
                        resting_order.order_id,
                        api::Price{best_level->first.value()},
                        api::Quantity{executed_quantity.value()}});
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
                if (order.order_type != api::OrderType::market &&
                    order.price > best_level->first)
                {
                    break;
                }

                auto &resting_orders = best_level->second;
                while (remaining_quantity > Quantity{0} && !resting_orders.empty())
                {
                    auto &resting_order = resting_orders.front();
                    const Quantity executed_quantity =
                        std::min(remaining_quantity, resting_order.remaining_quantity);
                    const domain::Trade trade{
                        next_execution_id_++,
                        order.instrument_id,
                        order.order_id,
                        resting_order.order_id,
                        best_level->first,
                        executed_quantity};
                    events.emplace_back(api::TradeExecuted{
                        trade,
                        order.instrument_id,
                        order.order_id,
                        resting_order.order_id,
                        api::Price{best_level->first.value()},
                        api::Quantity{executed_quantity.value()}});
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

        if (order.order_type == api::OrderType::ioc ||
            order.order_type == api::OrderType::fok ||
            order.order_type == api::OrderType::market)
        {
            return events;
        }

        if (remaining_quantity > Quantity{0})
        {
            const OrderLocation location{order.side, order.price};
            order_locations_.emplace(order.order_id, location);
            if (order.side == api::Side::buy)
            {
                buy_levels_[order.price].push_back(
                    RestingOrder{order.order_id, remaining_quantity, order.order_type});
            }
            else
            {
                sell_levels_[order.price].push_back(
                    RestingOrder{order.order_id, remaining_quantity, order.order_type});
            }
        }

        return events;
    }

    bool OrderBook::can_fully_match(const Order &order) const
    {
        Quantity remaining_quantity = order.quantity;

        if (order.side == api::Side::buy)
        {
            for (const auto &level : sell_levels_)
            {
                if (order.order_type != api::OrderType::market &&
                    order.price < level.first)
                {
                    break;
                }
                for (const auto &resting_order : level.second)
                {
                    if (resting_order.remaining_quantity > remaining_quantity ||
                        resting_order.remaining_quantity == remaining_quantity)
                    {
                        return true;
                    }
                    remaining_quantity -= resting_order.remaining_quantity;
                }
            }
        }
        else
        {
            for (const auto &level : buy_levels_)
            {
                if (order.order_type != api::OrderType::market &&
                    order.price > level.first)
                {
                    break;
                }
                for (const auto &resting_order : level.second)
                {
                    if (resting_order.remaining_quantity > remaining_quantity ||
                        resting_order.remaining_quantity == remaining_quantity)
                    {
                        return true;
                    }
                    remaining_quantity -= resting_order.remaining_quantity;
                }
            }
        }

        return false;
    }

    OrderBook::EventBatch OrderBook::cancel_order(const api::CancelOrder &request)
    {
        const auto location = order_locations_.find(request.order_id);
        if (location == order_locations_.end())
        {
            const domain::Order rejected_order{
                request.instrument_id,
                request.order_id,
                api::Side::buy,
                Price{0},
                Quantity{0},
                Quantity{0},
                domain::OrderStatus::rejected,
                api::OrderType::limit};
            return {api::OrderRejected{rejected_order, request.instrument_id, request.order_id,
                api::RejectReason::unknown_order_id}};
        }

        api::OrderType order_type{api::OrderType::limit};
        if (location->second.side == api::Side::buy)
        {
            const auto level = buy_levels_.find(location->second.price);
            if (level != buy_levels_.end())
            {
                for (const auto &resting_order : level->second)
                {
                    if (resting_order.order_id == request.order_id)
                    {
                        order_type = resting_order.order_type;
                        break;
                    }
                }
            }
        }
        else
        {
            const auto level = sell_levels_.find(location->second.price);
            if (level != sell_levels_.end())
            {
                for (const auto &resting_order : level->second)
                {
                    if (resting_order.order_id == request.order_id)
                    {
                        order_type = resting_order.order_type;
                        break;
                    }
                }
            }
        }

        remove_order_from_level(request.order_id, location->second);
        order_locations_.erase(location);
        const domain::Order canceled_order{
            request.instrument_id,
            request.order_id,
            location->second.side,
            location->second.price,
            Quantity{0},
            Quantity{0},
            domain::OrderStatus::canceled,
            order_type};
        return {api::OrderCanceled{canceled_order, request.instrument_id, request.order_id}};
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
