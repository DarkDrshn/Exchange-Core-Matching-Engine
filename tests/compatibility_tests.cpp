#include "exchange_core/engine/matching_engine.hpp"

#include <cassert>
#include <variant>

namespace
{

    using exchange_core::api::CancelOrder;
    using exchange_core::api::EngineEvent;
    using exchange_core::api::OrderAccepted;
    using exchange_core::api::OrderCanceled;
    using exchange_core::api::OrderRejected;
    using exchange_core::api::PlaceOrder;
    using exchange_core::api::RejectReason;
    using exchange_core::api::Side;
    using exchange_core::api::TradeExecuted;
    using exchange_core::engine::MatchingEngine;

    const auto &event_at(const MatchingEngine::EventBatch &events, std::size_t index)
    {
        assert(index < events.size());
        return events[index];
    }

    void accepts_resting_order()
    {
        MatchingEngine engine;
        const auto events = engine.place_order(PlaceOrder{101, Side::buy, 100, 10});

        assert(events.size() == 1);
        assert(std::holds_alternative<OrderAccepted>(event_at(events, 0)));
        assert(engine.contains_order(101));
    }

    void matches_at_resting_price()
    {
        MatchingEngine engine;
        engine.place_order(PlaceOrder{201, Side::sell, 100, 10});

        const auto events = engine.place_order(PlaceOrder{202, Side::buy, 105, 10});
        assert(events.size() == 2);
        assert(std::holds_alternative<OrderAccepted>(event_at(events, 0)));
        const auto &trade = std::get<TradeExecuted>(event_at(events, 1));
        assert(trade.incoming_order_id == 202);
        assert(trade.resting_order_id == 201);
        assert(trade.execution_price == 100);
        assert(trade.execution_quantity == 10);
        assert(!engine.contains_order(201));
        assert(!engine.contains_order(202));
    }

    void preserves_fifo_at_one_price()
    {
        MatchingEngine engine;
        engine.place_order(PlaceOrder{301, Side::sell, 100, 5});
        engine.place_order(PlaceOrder{302, Side::sell, 100, 5});

        const auto events = engine.place_order(PlaceOrder{303, Side::buy, 100, 7});
        assert(events.size() == 3);
        assert(std::get<TradeExecuted>(event_at(events, 1)).resting_order_id == 301);
        assert(std::get<TradeExecuted>(event_at(events, 1)).execution_quantity == 5);
        assert(std::get<TradeExecuted>(event_at(events, 2)).resting_order_id == 302);
        assert(std::get<TradeExecuted>(event_at(events, 2)).execution_quantity == 2);
        assert(engine.contains_order(302));
        assert(!engine.contains_order(303));
    }

    void rejects_duplicate_and_invalid_orders()
    {
        MatchingEngine engine;
        engine.place_order(PlaceOrder{401, Side::buy, 100, 1});

        const auto duplicate = engine.place_order(PlaceOrder{401, Side::sell, 101, 1});
        assert(std::get<OrderRejected>(event_at(duplicate, 0)).reason ==
               RejectReason::duplicate_order_id);

        const auto invalid = engine.place_order(PlaceOrder{402, Side::buy, 0, 1});
        assert(std::get<OrderRejected>(event_at(invalid, 0)).reason == RejectReason::invalid_order);
    }

    void cancels_resting_order_and_rejects_unknown_order()
    {
        MatchingEngine engine;
        engine.place_order(PlaceOrder{501, Side::buy, 100, 4});

        const auto canceled = engine.cancel_order(CancelOrder{501});
        assert(std::holds_alternative<OrderCanceled>(event_at(canceled, 0)));
        assert(!engine.contains_order(501));

        const auto rejected = engine.cancel_order(CancelOrder{501});
        assert(std::get<OrderRejected>(event_at(rejected, 0)).reason ==
               RejectReason::unknown_order_id);
    }

} // namespace

int main()
{
    accepts_resting_order();
    matches_at_resting_price();
    preserves_fifo_at_one_price();
    rejects_duplicate_and_invalid_orders();
    cancels_resting_order_and_rejects_unknown_order();
    return 0;
}
