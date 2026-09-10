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

    constexpr exchange_core::domain::InstrumentId primary_instrument_id = 1;

    void require(bool condition)
    {
        if (!condition)
        {
            std::abort();
        }
    }

    void register_primary_instrument(MatchingEngine &engine)
    {
        require(engine.register_instrument({
            primary_instrument_id,
            "PRIMARY",
            exchange_core::domain::Price{1},
            exchange_core::domain::Quantity{1}}));
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
            require(!engine_->contains_order(primary_instrument_id, 202));
            observed_resting_order_state_.push_back(
                engine_->contains_order(primary_instrument_id, 201));
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
        register_primary_instrument(engine);
        const auto events = engine.place_order(PlaceOrder{1, 101, Side::buy, 100, 10});

        require(events.size() == 1);
        require(std::holds_alternative<OrderAccepted>(event_at(events, 0)));
        require(engine.contains_order(1, 101));
    }

    void matches_at_resting_price()
    {
        MatchingEngine engine;
        register_primary_instrument(engine);
        engine.place_order(PlaceOrder{1, 201, Side::sell, 100, 10});

        const auto events = engine.place_order(PlaceOrder{1, 202, Side::buy, 105, 10});
        require(events.size() == 2);
        require(std::holds_alternative<OrderAccepted>(event_at(events, 0)));
        const auto &trade = std::get<TradeExecuted>(event_at(events, 1));
        require(trade.incoming_order_id == 202);
        require(trade.resting_order_id == 201);
        require(trade.execution_price == 100);
        require(trade.execution_quantity == 10);
        require(!engine.contains_order(1, 201));
        require(!engine.contains_order(1, 202));
    }

    void preserves_fifo_at_one_price()
    {
        MatchingEngine engine;
        register_primary_instrument(engine);
        engine.place_order(PlaceOrder{1, 301, Side::sell, 100, 5});
        engine.place_order(PlaceOrder{1, 302, Side::sell, 100, 5});

        const auto events = engine.place_order(PlaceOrder{1, 303, Side::buy, 100, 7});
        require(events.size() == 3);
        require(std::get<TradeExecuted>(event_at(events, 1)).resting_order_id == 301);
        require(std::get<TradeExecuted>(event_at(events, 1)).execution_quantity == 5);
        require(std::get<TradeExecuted>(event_at(events, 2)).resting_order_id == 302);
        require(std::get<TradeExecuted>(event_at(events, 2)).execution_quantity == 2);
        require(engine.contains_order(1, 302));
        require(!engine.contains_order(1, 303));
    }

    void rejects_duplicate_and_invalid_orders()
    {
        MatchingEngine engine;
        register_primary_instrument(engine);
        engine.place_order(PlaceOrder{1, 401, Side::buy, 100, 1});

        const auto duplicate = engine.place_order(PlaceOrder{1, 401, Side::sell, 101, 1});
        require(std::get<OrderRejected>(event_at(duplicate, 0)).reason ==
            RejectReason::duplicate_order_id);

        const auto invalid = engine.place_order(PlaceOrder{1, 402, Side::buy, 0, 1});
        require(std::get<OrderRejected>(event_at(invalid, 0)).reason == RejectReason::invalid_order);
    }

    void cancels_resting_order_and_rejects_unknown_order()
    {
        MatchingEngine engine;
        register_primary_instrument(engine);
        engine.place_order(PlaceOrder{1, 501, Side::buy, 100, 4});

        const auto canceled = engine.cancel_order(CancelOrder{1, 501});
        require(std::holds_alternative<OrderCanceled>(event_at(canceled, 0)));
        require(!engine.contains_order(1, 501));

        const auto rejected = engine.cancel_order(CancelOrder{1, 501});
        require(std::get<OrderRejected>(event_at(rejected, 0)).reason ==
            RejectReason::unknown_order_id);
    }

    void applies_engine_limits_at_the_api_boundary()
    {
        MatchingEngine engine(EngineConfig{100, 5});
        register_primary_instrument(engine);

        const auto price_rejected = engine.place_order(PlaceOrder{1, 601, Side::buy, 101, 1});
        require(std::get<OrderRejected>(event_at(price_rejected, 0)).reason ==
            RejectReason::invalid_order);

        const auto quantity_rejected = engine.place_order(PlaceOrder{1, 602, Side::buy, 100, 6});
        require(std::get<OrderRejected>(event_at(quantity_rejected, 0)).reason ==
            RejectReason::invalid_order);

        const auto accepted = engine.place_order(PlaceOrder{1, 603, Side::buy, 100, 5});
        require(std::holds_alternative<OrderAccepted>(event_at(accepted, 0)));
    }

    void publishes_events_after_mutation_to_a_reentrant_sink()
    {
        ReentrantEventSink sink;
        MatchingEngine engine({}, &sink);
        register_primary_instrument(engine);
        sink.set_engine(&engine);

        engine.place_order(PlaceOrder{1, 201, Side::sell, 100, 10});
        const auto events = engine.place_order(PlaceOrder{1, 202, Side::buy, 100, 10});

        require(events.size() == 2);
        require(sink.event_count() == 3);
        require(sink.observed_resting_order_state() == std::vector<bool>{true, false, false});
    }

    void isolates_identical_order_ids_between_instruments()
    {
        MatchingEngine engine;
        register_primary_instrument(engine);
        require(engine.register_instrument({
            2,
            "SECONDARY",
            exchange_core::domain::Price{1},
            exchange_core::domain::Quantity{1}}));

        require(engine.place_order(PlaceOrder{1, 701, Side::buy, 100, 5}).size() == 1);
        require(engine.place_order(PlaceOrder{2, 701, Side::sell, 100, 5}).size() == 1);
        require(engine.contains_order(1, 701));
        require(engine.cancel_order(CancelOrder{1, 701}).size() == 1);
        require(engine.contains_order(2, 701));
        require(engine.cancel_order(CancelOrder{2, 701}).size() == 1);
    }

    void rejects_unknown_instruments()
    {
        MatchingEngine engine;
        const auto events = engine.place_order(PlaceOrder{99, 801, Side::buy, 100, 1});
        require(std::get<OrderRejected>(event_at(events, 0)).reason ==
            RejectReason::unknown_instrument);
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
    isolates_identical_order_ids_between_instruments();
    rejects_unknown_instruments();
    return 0;
}
