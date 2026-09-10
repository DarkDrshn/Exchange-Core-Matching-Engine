#include "exchange_core/engine/matching_engine.hpp"

#include "domain/order_book.hpp"

namespace exchange_core::engine
{

    struct MatchingEngine::Impl
    {
        domain::OrderBook order_book;
        EngineConfig configuration;
        api::IEventSink *event_sink;
    };

    MatchingEngine::MatchingEngine(EngineConfig configuration, api::IEventSink *event_sink)
        : implementation_(std::make_unique<Impl>(Impl{{}, configuration, event_sink}))
    {
    }

    MatchingEngine::~MatchingEngine() = default;

    MatchingEngine::MatchingEngine(MatchingEngine &&) noexcept = default;

    MatchingEngine &MatchingEngine::operator=(MatchingEngine &&) noexcept = default;

    MatchingEngine::EventBatch MatchingEngine::place_order(const api::PlaceOrder &request)
    {
        if (request.price <= 0 || request.quantity == 0 ||
            request.price > implementation_->configuration.maximum_order_price ||
            request.quantity > implementation_->configuration.maximum_order_quantity)
        {
            const EventBatch events = {
                api::OrderRejected{request.order_id, api::RejectReason::invalid_order}};
            publish(events);
            return events;
        }

        const domain::Order order{
            request.order_id,
            request.side,
            domain::Price{request.price},
            domain::Quantity{request.quantity}};
        const EventBatch events = implementation_->order_book.place_order(order);
        publish(events);
        return events;
    }

    MatchingEngine::EventBatch MatchingEngine::cancel_order(const api::CancelOrder &request)
    {
        const EventBatch events = implementation_->order_book.cancel_order(request);
        publish(events);
        return events;
    }

    bool MatchingEngine::contains_order(api::OrderId order_id) const
    {
        return implementation_->order_book.contains_order(order_id);
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
