#pragma once

#include "exchange_core/api/events.hpp"
#include "exchange_core/api/event_sink.hpp"
#include "exchange_core/engine/engine_config.hpp"
#include "exchange_core/domain/instrument_registry.hpp"

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

        bool register_instrument(domain::Instrument instrument);
        [[nodiscard]] const domain::Instrument *find_instrument(
            domain::InstrumentId instrument_id) const;

        // cppcheck-suppress syntaxError
        [[nodiscard]] bool contains_order(
            domain::InstrumentId instrument_id, api::OrderId order_id) const;
        [[nodiscard]] std::int64_t account_position(
            api::AccountId account_id, domain::InstrumentId instrument_id) const;
        [[nodiscard]] api::Quantity account_open_order_quantity(
            api::AccountId account_id) const;
        [[nodiscard]] api::Quantity account_reserved_margin(
            api::AccountId account_id) const;

    private:
        struct Impl;
        std::unique_ptr<Impl> implementation_;

        void publish(const EventBatch &events) const;
    };

} // namespace exchange_core::engine
