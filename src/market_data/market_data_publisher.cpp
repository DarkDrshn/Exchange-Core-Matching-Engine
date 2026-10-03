#include "exchange_core/market_data/market_data_publisher.hpp"

#include <variant>
#include <type_traits>

namespace exchange_core::market_data
{
    void MarketDataPublisher::on_event(const api::EngineEvent &event)
    {
        std::visit(
            [this](const auto &typed_event)
            {
                using EventType = std::decay_t<decltype(typed_event)>;
                if constexpr (std::is_same_v<EventType, api::OrderAccepted>)
                {
                    add_order(typed_event);
                }
                else if constexpr (std::is_same_v<EventType, api::TradeExecuted>)
                {
                    apply_trade(typed_event);
                }
                else if constexpr (std::is_same_v<EventType, api::OrderCanceled>)
                {
                    remove_order(typed_event);
                }
            },
            event);
    }

    void MarketDataPublisher::on_events(const std::vector<api::EngineEvent> &events)
    {
        batching_ = true;
        for (const auto &event : events)
        {
            on_event(event);
        }
        batching_ = false;
        flush_pending_levels();
    }

    void MarketDataPublisher::add_order(const api::OrderAccepted &accepted)
    {
        if (accepted.order.order_type == api::OrderType::market ||
            accepted.order.order_type == api::OrderType::ioc ||
            accepted.order.order_type == api::OrderType::fok)
        {
            return;
        }

        const OrderKey key{accepted.instrument_id, accepted.order_id};
        orders_[key] = TrackedOrder{
            accepted.order.side,
            accepted.order.price,
            accepted.order.remaining_quantity};
        auto &aggregate = level_quantities_[level_key(
            accepted.instrument_id, accepted.order.side, accepted.order.price)];
        aggregate += accepted.order.remaining_quantity.value();
        emit_level(accepted.instrument_id, accepted.order.side, accepted.order.price);
    }

    void MarketDataPublisher::apply_trade(const api::TradeExecuted &trade)
    {
        const OrderKey incoming_key{trade.instrument_id, trade.incoming_order_id};
        const OrderKey resting_key{trade.instrument_id, trade.resting_order_id};
        const auto incoming = orders_.find(incoming_key);
        if (incoming != orders_.end())
        {
            incoming->second.remaining_quantity -= domain::Quantity{trade.execution_quantity};
            if (incoming->second.remaining_quantity == domain::Quantity{0})
            {
                const auto side = incoming->second.side;
                const auto price = incoming->second.price;
                orders_.erase(incoming);
                auto &aggregate = level_quantities_[level_key(
                    trade.instrument_id, side, price)];
                aggregate -= trade.execution_quantity;
                emit_level(trade.instrument_id, side, price);
            }
        }

        const auto resting = orders_.find(resting_key);
        if (resting != orders_.end())
        {
            const auto side = resting->second.side;
            const auto price = resting->second.price;
            resting->second.remaining_quantity -= domain::Quantity{trade.execution_quantity};
            auto &aggregate = level_quantities_[level_key(
                trade.instrument_id, side, price)];
            aggregate -= trade.execution_quantity;
            emit_level(trade.instrument_id, side, price);
            if (resting->second.remaining_quantity == domain::Quantity{0})
            {
                orders_.erase(resting);
            }
        }
    }

    void MarketDataPublisher::remove_order(const api::OrderCanceled &canceled)
    {
        const OrderKey key{canceled.instrument_id, canceled.order_id};
        const auto order = orders_.find(key);
        if (order == orders_.end())
        {
            return;
        }

        const auto side = order->second.side;
        const auto price = order->second.price;
        auto &aggregate = level_quantities_[level_key(
            canceled.instrument_id, side, price)];
        aggregate -= order->second.remaining_quantity.value();
        orders_.erase(order);
        emit_level(canceled.instrument_id, side, price);
    }

    void MarketDataPublisher::emit_level(
        domain::InstrumentId instrument_id, api::Side side, domain::Price price)
    {
        const auto key = level_key(instrument_id, side, price);
        if (batching_)
        {
            pending_levels_[key] = true;
            return;
        }
        const auto aggregate = level_quantities_.find(key);
        const auto quantity = aggregate == level_quantities_.end()
            ? api::Quantity{0}
            : aggregate->second;
        sink_.on_level_update(Level2Update{
            next_sequence_++, instrument_id, side, price, domain::Quantity{quantity}});
    }

    void MarketDataPublisher::flush_pending_levels()
    {
        for (const auto &pending : pending_levels_)
        {
            const auto &key = pending.first;
            emit_level(key.instrument_id, key.side, key.price);
        }
        pending_levels_.clear();
    }

    MarketDataPublisher::LevelKey MarketDataPublisher::level_key(
        domain::InstrumentId instrument_id, api::Side side, domain::Price price) const
    {
        return LevelKey{instrument_id, side, price};
    }

} // namespace exchange_core::market_data
