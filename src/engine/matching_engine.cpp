#include "exchange_core/engine/matching_engine.hpp"
#include "exchange_core/risk/risk_manager.hpp"

#include "domain/order_book.hpp"

#include <unordered_map>

namespace exchange_core::engine
{

    struct MatchingEngine::Impl
    {
        EngineConfig configuration;
        api::IEventSink *event_sink;
        domain::InstrumentRegistry instruments;
        std::unordered_map<domain::InstrumentId, domain::OrderBook> order_books;
        risk::RiskManager risk_manager;
    };

    MatchingEngine::MatchingEngine(EngineConfig configuration, api::IEventSink *event_sink)
        : implementation_(std::make_unique<Impl>(Impl{
              configuration, event_sink, {}, {}, risk::RiskManager{configuration}}))
    {
    }

    MatchingEngine::~MatchingEngine() = default;

    MatchingEngine::MatchingEngine(MatchingEngine &&) noexcept = default;

    MatchingEngine &MatchingEngine::operator=(MatchingEngine &&) noexcept = default;

    MatchingEngine::EventBatch MatchingEngine::place_order(const api::PlaceOrder &request)
    {
        const auto instrument = implementation_->instruments.find(request.instrument_id);
        if (instrument == nullptr)
        {
            const domain::Order rejected_order{
                request.instrument_id,
                request.order_id,
                request.side,
                domain::Price{0},
                domain::Quantity{0},
                domain::Quantity{0},
                domain::OrderStatus::rejected,
                request.order_type};
            const EventBatch events = {api::OrderRejected{
                rejected_order, request.instrument_id, request.order_id,
                api::RejectReason::unknown_instrument}};
            publish(events);
            return events;
        }
        const bool valid_market_price = request.order_type == api::OrderType::market &&
            request.price == 0;
        const bool valid_limit_price = request.order_type != api::OrderType::market &&
            request.price > 0;
        if ((!valid_market_price && !valid_limit_price) || request.quantity == 0)
        {
            const domain::Order rejected_order{
                request.instrument_id,
                request.order_id,
                request.side,
                domain::Price{0},
                domain::Quantity{0},
                domain::Quantity{0},
                domain::OrderStatus::rejected,
                request.order_type};
            const EventBatch events = {api::OrderRejected{
                rejected_order, request.instrument_id, request.order_id,
                api::RejectReason::invalid_order}};
            publish(events);
            return events;
        }

        const auto risk_rejection = implementation_->risk_manager.evaluate(request);
        if (risk_rejection != risk::RejectReason::none)
        {
            api::RejectReason reason = api::RejectReason::risk_quantity_limit;
            if (risk_rejection == risk::RejectReason::notional_limit)
            {
                reason = api::RejectReason::risk_notional_limit;
            }
            else if (risk_rejection == risk::RejectReason::fat_finger_limit)
            {
                reason = api::RejectReason::risk_fat_finger_limit;
            }

            const domain::Order rejected_order{
                request.instrument_id,
                request.order_id,
                request.side,
                domain::Price{request.price},
                domain::Quantity{request.quantity},
                domain::Quantity{request.quantity},
                domain::OrderStatus::rejected,
                request.order_type};
            const EventBatch events = {api::OrderRejected{
                rejected_order, request.instrument_id, request.order_id, reason}};
            publish(events);
            return events;
        }

        const domain::Order order{
            request.instrument_id,
            request.order_id,
            request.side,
            domain::Price{request.price},
            domain::Quantity{request.quantity},
            domain::Quantity{request.quantity},
            domain::OrderStatus::new_order,
            request.order_type};
        const EventBatch events = implementation_->order_books.at(request.instrument_id)
            .place_order(order);
        publish(events);
        return events;
    }

    MatchingEngine::EventBatch MatchingEngine::cancel_order(const api::CancelOrder &request)
    {
        const auto instrument = implementation_->instruments.find(request.instrument_id);
        if (instrument == nullptr)
        {
            const domain::Order rejected_order{
                request.instrument_id,
                request.order_id,
                api::Side::buy,
                domain::Price{0},
                domain::Quantity{0},
                domain::Quantity{0},
                domain::OrderStatus::rejected};
            const EventBatch events = {api::OrderRejected{
                rejected_order, request.instrument_id, request.order_id,
                api::RejectReason::unknown_instrument}};
            publish(events);
            return events;
        }
        const EventBatch events = implementation_->order_books.at(request.instrument_id)
            .cancel_order(request);
        publish(events);
        return events;
    }

    bool MatchingEngine::register_instrument(domain::Instrument instrument)
    {
        if (!implementation_->instruments.add(instrument))
        {
            return false;
        }

        implementation_->order_books.emplace(
            instrument.instrument_id, domain::OrderBook{});
        return true;
    }

    const domain::Instrument *MatchingEngine::find_instrument(
        domain::InstrumentId instrument_id) const
    {
        return implementation_->instruments.find(instrument_id);
    }

    bool MatchingEngine::contains_order(
        domain::InstrumentId instrument_id, api::OrderId order_id) const
    {
        const auto order_book = implementation_->order_books.find(instrument_id);
        return order_book != implementation_->order_books.end() &&
               order_book->second.contains_order(order_id);
    }

    void MatchingEngine::publish(const EventBatch &events) const
    {
        if (implementation_->event_sink == nullptr)
        {
            return;
        }

        for (const auto &event : events)
        {
            implementation_->event_sink->on_event(event);
        }
    }

} // namespace exchange_core::engine
