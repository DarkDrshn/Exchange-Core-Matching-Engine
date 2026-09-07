#include "exchange_core/engine/matching_engine.hpp"

#include "domain/order_book.hpp"

namespace exchange_core::engine
{

    struct MatchingEngine::Impl
    {
        domain::OrderBook order_book;
    };

    MatchingEngine::MatchingEngine()
        : implementation_(std::make_unique<Impl>())
    {
    }

    MatchingEngine::~MatchingEngine() = default;

    MatchingEngine::MatchingEngine(MatchingEngine &&) noexcept = default;

    MatchingEngine &MatchingEngine::operator=(MatchingEngine &&) noexcept = default;

    MatchingEngine::EventBatch MatchingEngine::place_order(const api::PlaceOrder &request)
    {
        return implementation_->order_book.place_order(request);
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
