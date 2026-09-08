#include "exchange_core/engine/matching_engine.hpp"

#include "domain/order_book.hpp"

namespace exchange_core::engine
{

    struct MatchingEngine::Impl
    {
        domain::OrderBook order_book;
        EngineConfig configuration;
    };

    MatchingEngine::MatchingEngine(EngineConfig configuration)
        : implementation_(std::make_unique<Impl>(Impl{{}, configuration}))
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
            return {api::OrderRejected{request.order_id, api::RejectReason::invalid_order}};
        }

        const domain::Order order{
            request.order_id,
            request.side,
            domain::Price{request.price},
            domain::Quantity{request.quantity}};
        return implementation_->order_book.place_order(order);
    }

    MatchingEngine::EventBatch MatchingEngine::cancel_order(const api::CancelOrder &request)
    {
        return implementation_->order_book.cancel_order(request);
    }

    bool MatchingEngine::contains_order(api::OrderId order_id) const
    {
        return implementation_->order_book.contains_order(order_id);
    }

} // namespace exchange_core::engine
