#include "exchange_core/engine/matching_engine.hpp"

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
    };

    MatchingEngine::MatchingEngine(EngineConfig configuration, api::IEventSink *event_sink)
        : implementation_(std::make_unique<Impl>(Impl{configuration, event_sink, {}, {}}))
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
            const EventBatch events = {api::OrderRejected{
                request.instrument_id, request.order_id, api::RejectReason::unknown_instrument}};
            publish(events);
            return events;
        }
        if (request.price <= 0 || request.quantity == 0 ||
            request.price > implementation_->configuration.maximum_order_price ||
            request.quantity > implementation_->configuration.maximum_order_quantity)
        {
            const EventBatch events = {
                api::OrderRejected{
                    request.instrument_id, request.order_id, api::RejectReason::invalid_order}};
            publish(events);
            return events;
        }

        const domain::Order order{
            request.instrument_id,
            request.order_id,
            request.side,
            domain::Price{request.price},
            domain::Quantity{request.quantity}};
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
            const EventBatch events = {api::OrderRejected{
                request.instrument_id, request.order_id, api::RejectReason::unknown_instrument}};
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
