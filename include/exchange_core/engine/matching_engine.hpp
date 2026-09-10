#pragma once

#include "exchange_core/api/events.hpp"
#include "exchange_core/api/event_sink.hpp"
#include "exchange_core/engine/engine_config.hpp"

#include <memory>
#include <vector>

namespace exchange_core::engine
{

    class MatchingEngine
    {
    public:
        using EventBatch = std::vector<api::EngineEvent>;

        explicit MatchingEngine(
            EngineConfig configuration = {},
            api::IEventSink *event_sink = nullptr);
        ~MatchingEngine();

        MatchingEngine(const MatchingEngine &) = delete;
        MatchingEngine &operator=(const MatchingEngine &) = delete;
        MatchingEngine(MatchingEngine &&) noexcept;
        MatchingEngine &operator=(MatchingEngine &&) noexcept;

        EventBatch place_order(const api::PlaceOrder &request);
        EventBatch cancel_order(const api::CancelOrder &request);

        // cppcheck-suppress syntaxError
        [[nodiscard]] bool contains_order(api::OrderId order_id) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> implementation_;

        void publish(const EventBatch &events) const;
    };

} // namespace exchange_core::engine
