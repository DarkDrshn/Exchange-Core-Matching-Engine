#include "exchange_core/engine/matching_engine.hpp"

#include <cstdlib>
#include <variant>
#include <vector>

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
    using exchange_core::engine::EngineConfig;
    using exchange_core::engine::MatchingEngine;

    void require(bool condition)
    {
        if (!condition)
        {
            std::abort();
        }
    }

    class ReentrantEventSink final : public exchange_core::api::IEventSink
    {
    public:
        void set_engine(const MatchingEngine *engine)
        {
            engine_ = engine;
        }

        void on_event(const EngineEvent &event) override
        {
            require(engine_ != nullptr);
            require(!std::holds_alternative<OrderRejected>(event));
            ++event_count_;
            require(!engine_->contains_order(202));
            observed_resting_order_state_.push_back(engine_->contains_order(201));
        }

        [[nodiscard]] std::size_t event_count() const
        {
            return event_count_;
        }

        [[nodiscard]] const std::vector<bool> &observed_resting_order_state() const
        {
            return observed_resting_order_state_;
        }

    private:
        const MatchingEngine *engine_{nullptr};
        std::size_t event_count_{0};
        std::vector<bool> observed_resting_order_state_;
    };

    const auto &event_at(const MatchingEngine::EventBatch &events, std::size_t index)
    {
        require(index < events.size());
        return events[index];
    }

    void accepts_resting_order()
    {
        MatchingEngine engine;
        const auto events = engine.place_order(PlaceOrder{101, Side::buy, 100, 10});

        require(events.size() == 1);
        require(std::holds_alternative<OrderAccepted>(event_at(events, 0)));
        require(engine.contains_order(101));
    }

    void matches_at_resting_price()
    {
        MatchingEngine engine;
        engine.place_order(PlaceOrder{201, Side::sell, 100, 10});

        const auto events = engine.place_order(PlaceOrder{202, Side::buy, 105, 10});
        require(events.size() == 2);
        require(std::holds_alternative<OrderAccepted>(event_at(events, 0)));
        const auto &trade = std::get<TradeExecuted>(event_at(events, 1));
        require(trade.incoming_order_id == 202);
        require(trade.resting_order_id == 201);
        require(trade.execution_price == 100);
        require(trade.execution_quantity == 10);
        require(!engine.contains_order(201));
        require(!engine.contains_order(202));
    }

    void preserves_fifo_at_one_price()
    {
        MatchingEngine engine;
        engine.place_order(PlaceOrder{301, Side::sell, 100, 5});
        engine.place_order(PlaceOrder{302, Side::sell, 100, 5});

        const auto events = engine.place_order(PlaceOrder{303, Side::buy, 100, 7});
        require(events.size() == 3);
        require(std::get<TradeExecuted>(event_at(events, 1)).resting_order_id == 301);
        require(std::get<TradeExecuted>(event_at(events, 1)).execution_quantity == 5);
        require(std::get<TradeExecuted>(event_at(events, 2)).resting_order_id == 302);
        require(std::get<TradeExecuted>(event_at(events, 2)).execution_quantity == 2);
        require(engine.contains_order(302));
        require(!engine.contains_order(303));
    }

    void rejects_duplicate_and_invalid_orders()
    {
        MatchingEngine engine;
        engine.place_order(PlaceOrder{401, Side::buy, 100, 1});

        const auto duplicate = engine.place_order(PlaceOrder{401, Side::sell, 101, 1});
        require(std::get<OrderRejected>(event_at(duplicate, 0)).reason ==
            RejectReason::duplicate_order_id);

        const auto invalid = engine.place_order(PlaceOrder{402, Side::buy, 0, 1});
        require(std::get<OrderRejected>(event_at(invalid, 0)).reason == RejectReason::invalid_order);
    }

    void cancels_resting_order_and_rejects_unknown_order()
    {
        MatchingEngine engine;
        engine.place_order(PlaceOrder{501, Side::buy, 100, 4});

        const auto canceled = engine.cancel_order(CancelOrder{501});
        require(std::holds_alternative<OrderCanceled>(event_at(canceled, 0)));
        require(!engine.contains_order(501));

        const auto rejected = engine.cancel_order(CancelOrder{501});
        require(std::get<OrderRejected>(event_at(rejected, 0)).reason ==
            RejectReason::unknown_order_id);
    }

    void applies_engine_limits_at_the_api_boundary()
    {
        MatchingEngine engine(EngineConfig{100, 5});

        const auto price_rejected = engine.place_order(PlaceOrder{601, Side::buy, 101, 1});
        require(std::get<OrderRejected>(event_at(price_rejected, 0)).reason ==
            RejectReason::invalid_order);

        const auto quantity_rejected = engine.place_order(PlaceOrder{602, Side::buy, 100, 6});
        require(std::get<OrderRejected>(event_at(quantity_rejected, 0)).reason ==
            RejectReason::invalid_order);

        const auto accepted = engine.place_order(PlaceOrder{603, Side::buy, 100, 5});
        require(std::holds_alternative<OrderAccepted>(event_at(accepted, 0)));
    }

    void publishes_events_after_mutation_to_a_reentrant_sink()
    {
        ReentrantEventSink sink;
        MatchingEngine engine({}, &sink);
        sink.set_engine(&engine);

        engine.place_order(PlaceOrder{201, Side::sell, 100, 10});
        const auto events = engine.place_order(PlaceOrder{202, Side::buy, 100, 10});

        require(events.size() == 2);
        require(sink.event_count() == 3);
        require(sink.observed_resting_order_state() == std::vector<bool>{true, false, false});
    }

} // namespace

int main()
{
    accepts_resting_order();
    matches_at_resting_price();
    preserves_fifo_at_one_price();
    rejects_duplicate_and_invalid_orders();
    cancels_resting_order_and_rejects_unknown_order();
    applies_engine_limits_at_the_api_boundary();
    publishes_events_after_mutation_to_a_reentrant_sink();
    return 0;
}
