#include "exchange_core/engine/matching_engine.hpp"
#include "exchange_core/gateway/order_gateway.hpp"
#include "exchange_core/market_data/market_data_publisher.hpp"

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
    using exchange_core::api::OrderType;
    using exchange_core::api::PlaceOrder;
    using exchange_core::api::RejectReason;
    using exchange_core::api::Side;
    using exchange_core::api::TradeExecuted;
    using exchange_core::domain::ExecutionId;
    using exchange_core::domain::Order;
    using exchange_core::domain::OrderStatus;
    using exchange_core::domain::Price;
    using exchange_core::domain::Quantity;
    using exchange_core::domain::Trade;
    using exchange_core::engine::EngineConfig;
    using exchange_core::engine::MatchingEngine;
    using exchange_core::gateway::CancelOrderCommand;
    using exchange_core::gateway::OrderGateway;
    using exchange_core::gateway::PlaceOrderCommand;
    using exchange_core::market_data::IMarketDataSink;
    using exchange_core::market_data::Level2Update;
    using exchange_core::market_data::MarketDataPublisher;

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

    void validates_order_lifecycle_types()
    {
        Order order{1, 101, Side::buy, Price{100}, Quantity{10}, Quantity{10}, OrderStatus::new_order};
        require(order.remaining_quantity == Quantity{10});
        require(order.status == OrderStatus::new_order);

        Trade trade{ExecutionId{42}, 1, 101, 202, Price{100}, Quantity{4}};
        require(trade.execution_id == ExecutionId{42});
        require(trade.execution_quantity == Quantity{4});
        require(trade.incoming_order_id == 101);
        require(trade.resting_order_id == 202);
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

    void implements_ioc_and_post_only_orders()
    {
        MatchingEngine engine;
        register_primary_instrument(engine);
        engine.place_order(PlaceOrder{1, 901, Side::sell, 100, 10, OrderType::limit});

        const auto ioc_fill = engine.place_order(PlaceOrder{1, 902, Side::buy, 100, 6, OrderType::ioc});
        require(ioc_fill.size() == 2);
        require(std::holds_alternative<TradeExecuted>(event_at(ioc_fill, 1)));
        require(!engine.contains_order(1, 902));

        const auto post_only_rejected = engine.place_order(PlaceOrder{1, 903, Side::buy, 101, 4, OrderType::post_only});
        require(std::get<OrderRejected>(event_at(post_only_rejected, 0)).reason ==
            RejectReason::post_only_rejected);

        const auto post_only_pass = engine.place_order(PlaceOrder{1, 904, Side::buy, 99, 4, OrderType::post_only});
        require(std::holds_alternative<OrderAccepted>(event_at(post_only_pass, 0)));
        require(engine.contains_order(1, 904));
    }

    void implements_market_and_fill_or_kill_orders()
    {
        MatchingEngine engine;
        register_primary_instrument(engine);
        engine.place_order(PlaceOrder{1, 1001, Side::sell, 100, 3, OrderType::limit});
        engine.place_order(PlaceOrder{1, 1002, Side::sell, 101, 4, OrderType::limit});

        const auto market_fill = engine.place_order(
            PlaceOrder{1, 1003, Side::buy, 0, 3, OrderType::market});
        require(market_fill.size() == 2);
        require(std::get<TradeExecuted>(event_at(market_fill, 1)).execution_price == 100);
        require(!engine.contains_order(1, 1003));
        require(engine.contains_order(1, 1002));

        const auto fok_rejected = engine.place_order(
            PlaceOrder{1, 1004, Side::buy, 101, 5, OrderType::fok});
        require(fok_rejected.size() == 1);
        require(std::get<OrderRejected>(event_at(fok_rejected, 0)).reason ==
            RejectReason::fok_not_filled);
        require(engine.contains_order(1, 1002));

        const auto fok_fill = engine.place_order(
            PlaceOrder{1, 1005, Side::buy, 101, 4, OrderType::fok});
        require(fok_fill.size() == 2);
        require(std::holds_alternative<TradeExecuted>(event_at(fok_fill, 1)));
        require(!engine.contains_order(1, 1002));
        require(!engine.contains_order(1, 1005));
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
        MatchingEngine engine(EngineConfig{100, 10, 500});
        register_primary_instrument(engine);

        const auto price_rejected = engine.place_order(PlaceOrder{1, 601, Side::buy, 101, 1});
        require(std::get<OrderRejected>(event_at(price_rejected, 0)).reason ==
            RejectReason::risk_fat_finger_limit);

        const auto quantity_rejected = engine.place_order(PlaceOrder{1, 602, Side::buy, 100, 11});
        require(std::get<OrderRejected>(event_at(quantity_rejected, 0)).reason ==
            RejectReason::risk_quantity_limit);

        const auto notional_rejected = engine.place_order(PlaceOrder{1, 603, Side::buy, 100, 6});
        require(std::get<OrderRejected>(event_at(notional_rejected, 0)).reason ==
            RejectReason::risk_notional_limit);

        const auto accepted = engine.place_order(PlaceOrder{1, 604, Side::buy, 100, 5});
        require(std::holds_alternative<OrderAccepted>(event_at(accepted, 0)));
    }

    void tracks_account_reservations_positions_and_ownership()
    {
        MatchingEngine engine(EngineConfig{100, 20, 10000, 5, 20});
        register_primary_instrument(engine);

        const auto resting = engine.place_order(
            PlaceOrder{1, 1101, Side::buy, 100, 4, OrderType::limit, 77});
        require(std::holds_alternative<OrderAccepted>(event_at(resting, 0)));
        require(engine.account_open_order_quantity(77) == 4);

        const auto duplicate = engine.place_order(
            PlaceOrder{1, 1101, Side::sell, 100, 1, OrderType::limit, 88});
        require(std::get<OrderRejected>(event_at(duplicate, 0)).reason ==
            RejectReason::duplicate_order_id);
        require(engine.account_open_order_quantity(77) == 4);

        const auto second_order = engine.place_order(
            PlaceOrder{1, 1102, Side::buy, 100, 2, OrderType::limit, 77});
        require(std::get<OrderRejected>(event_at(second_order, 0)).reason ==
            RejectReason::risk_position_limit);
        require(engine.account_open_order_quantity(77) == 4);

        const auto unauthorized_cancel = engine.cancel_order(CancelOrder{1, 1101, 88});
        require(std::get<OrderRejected>(event_at(unauthorized_cancel, 0)).reason ==
            RejectReason::unauthorized_order);
        require(engine.account_open_order_quantity(77) == 4);

        engine.place_order(PlaceOrder{1, 1103, Side::sell, 100, 4, OrderType::limit, 88});
        require(engine.account_position(77, 1) == 4);
        require(engine.account_position(88, 1) == -4);
        require(engine.account_open_order_quantity(77) == 0);
        require(engine.account_open_order_quantity(88) == 0);
    }

    void applies_credit_limits_and_releases_margin_reservations()
    {
        MatchingEngine engine(EngineConfig{100, 20, 10000, 20, 20, 500, 5000});
        register_primary_instrument(engine);

        engine.place_order(PlaceOrder{1, 1201, Side::buy, 100, 4, OrderType::limit, 91});
        require(engine.account_reserved_margin(91) == 200);

        const auto credit_rejected = engine.place_order(
            PlaceOrder{1, 1202, Side::buy, 100, 7, OrderType::limit, 91});
        require(std::get<OrderRejected>(event_at(credit_rejected, 0)).reason ==
            RejectReason::risk_credit_limit);
        require(engine.account_reserved_margin(91) == 200);

        const auto canceled = engine.cancel_order(CancelOrder{1, 1201, 91});
        require(std::holds_alternative<OrderCanceled>(event_at(canceled, 0)));
        require(engine.account_reserved_margin(91) == 0);

        engine.place_order(PlaceOrder{1, 1203, Side::buy, 100, 4, OrderType::limit, 91});
        engine.place_order(PlaceOrder{1, 1204, Side::sell, 100, 4, OrderType::limit, 92});
        require(engine.account_reserved_margin(91) == 0);
        require(engine.account_reserved_margin(92) == 0);
    }

    void enforces_gateway_identity_and_request_sequences()
    {
        MatchingEngine engine;
        register_primary_instrument(engine);
        OrderGateway gateway(engine);
        require(gateway.register_client(501, 77));
        OrderGateway gateway_without_reference(engine);
        require(gateway_without_reference.register_client(502, 77));
        const auto missing_reference = gateway_without_reference.place_order(
            PlaceOrderCommand{502, 1, PlaceOrder{1, 1300, Side::buy, 100, 1,
                OrderType::limit, 77}});
        require(std::get<OrderRejected>(event_at(missing_reference, 0)).reason ==
            RejectReason::risk_reference_price_unavailable);
        require(gateway_without_reference.next_sequence(502) == 1);
        require(gateway.set_reference_price(1, 100));
        require(gateway.next_sequence(501) == 1);

        const auto accepted = gateway.place_order(
            PlaceOrderCommand{501, 1, PlaceOrder{1, 1301, Side::buy, 100, 2,
                OrderType::limit, 77}});
        require(std::holds_alternative<OrderAccepted>(event_at(accepted, 0)));
        require(gateway.next_sequence(501) == 2);

        const auto repeated_sequence = gateway.place_order(
            PlaceOrderCommand{501, 1, PlaceOrder{1, 1302, Side::buy, 100, 1,
                OrderType::limit, 77}});
        require(std::get<OrderRejected>(event_at(repeated_sequence, 0)).reason ==
            RejectReason::invalid_request_sequence);
        require(gateway.next_sequence(501) == 2);

        const auto wrong_account = gateway.place_order(
            PlaceOrderCommand{501, 2, PlaceOrder{1, 1303, Side::buy, 100, 1,
                OrderType::limit, 88}});
        require(std::get<OrderRejected>(event_at(wrong_account, 0)).reason ==
            RejectReason::unauthorized_order);
        require(gateway.next_sequence(501) == 2);

        const auto invalid_engine_order = gateway.place_order(
            PlaceOrderCommand{501, 2, PlaceOrder{1, 1304, Side::buy, 0, 1,
                OrderType::limit, 77}});
        require(std::get<OrderRejected>(event_at(invalid_engine_order, 0)).reason ==
            RejectReason::invalid_order);
        require(gateway.next_sequence(501) == 3);

        const auto canceled = gateway.cancel_order(
            CancelOrderCommand{501, 3, CancelOrder{1, 1301, 77}});
        require(std::holds_alternative<OrderCanceled>(event_at(canceled, 0)));
        require(gateway.next_sequence(501) == 4);

        const auto fat_finger_rejected = gateway.place_order(
            PlaceOrderCommand{501, 4, PlaceOrder{1, 1305, Side::buy, 120, 1,
                OrderType::limit, 77}});
        require(std::get<OrderRejected>(event_at(fat_finger_rejected, 0)).reason ==
            RejectReason::risk_fat_finger_limit);
        require(gateway.next_sequence(501) == 4);

        const auto unknown_client = gateway.place_order(
            PlaceOrderCommand{999, 1, PlaceOrder{1, 1306, Side::buy, 100, 1,
                OrderType::limit, 77}});
        require(std::get<OrderRejected>(event_at(unknown_client, 0)).reason ==
            RejectReason::unknown_client);
    }

    class RecordingMarketDataSink final : public IMarketDataSink
    {
    public:
        void on_level_update(const Level2Update &update) override
        {
            updates.push_back(update);
        }

        std::vector<Level2Update> updates;
    };

    void publishes_monotonic_level_two_updates()
    {
        RecordingMarketDataSink sink;
        MarketDataPublisher publisher(sink);

        const Order first_order{
            1, 1401, Side::buy, Price{100}, Quantity{4}, Quantity{4},
            OrderStatus::new_order, OrderType::limit, 71};
        const Order second_order{
            1, 1402, Side::buy, Price{100}, Quantity{3}, Quantity{3},
            OrderStatus::new_order, OrderType::limit, 72};
        publisher.on_event(OrderAccepted{first_order, 1, 1401});
        publisher.on_event(OrderAccepted{second_order, 1, 1402});
        require(sink.updates.size() == 2);
        require(sink.updates[0].aggregate_quantity == Quantity{4});
        require(sink.updates[1].aggregate_quantity == Quantity{7});
        require(sink.updates[0].sequence == 1);
        require(sink.updates[1].sequence == 2);

        const Trade trade{
            ExecutionId{51}, 1, 9001, 1401, Price{100}, Quantity{2}};
        publisher.on_event(TradeExecuted{
            trade, 1, 9001, 1401, 100, 2});
        require(sink.updates.back().aggregate_quantity == Quantity{5});
        require(sink.updates.back().sequence == 3);

        publisher.on_event(OrderCanceled{
            second_order, 1, 1402});
        require(sink.updates.back().aggregate_quantity == Quantity{2});
        require(sink.updates.back().sequence == 4);

        publisher.on_event(OrderRejected{
            first_order, 1, 1401, RejectReason::invalid_order});
        require(sink.updates.size() == 4);

        RecordingMarketDataSink batch_sink;
        MarketDataPublisher batch_publisher(batch_sink);
        const Trade batch_trade{
            ExecutionId{52}, 1, 9002, 1401, Price{100}, Quantity{4}};
        batch_publisher.on_events({
            OrderAccepted{first_order, 1, 1401},
            TradeExecuted{batch_trade, 1, 9002, 1401, 100, 4}});
        require(batch_sink.updates.size() == 1);
        require(batch_sink.updates[0].aggregate_quantity == Quantity{0});
        require(batch_sink.updates[0].sequence == 1);
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
    validates_order_lifecycle_types();
    accepts_resting_order();
    matches_at_resting_price();
    preserves_fifo_at_one_price();
    rejects_duplicate_and_invalid_orders();
    implements_ioc_and_post_only_orders();
    implements_market_and_fill_or_kill_orders();
    cancels_resting_order_and_rejects_unknown_order();
    applies_engine_limits_at_the_api_boundary();
    tracks_account_reservations_positions_and_ownership();
    applies_credit_limits_and_releases_margin_reservations();
    enforces_gateway_identity_and_request_sequences();
    publishes_monotonic_level_two_updates();
    publishes_events_after_mutation_to_a_reentrant_sink();
    isolates_identical_order_ids_between_instruments();
    rejects_unknown_instruments();
    return 0;
}
