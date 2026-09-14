#pragma once

#include "exchange_core/api/events.hpp"
#include "exchange_core/api/event_sink.hpp"

#include <cstdint>
#include <map>
#include <unordered_map>
#include <vector>

namespace exchange_core::market_data
{
    struct Level2Update
    {
        std::uint64_t sequence{};
        domain::InstrumentId instrument_id{};
        api::Side side{};
        domain::Price price{0};
        domain::Quantity aggregate_quantity{0};
    };

    class IMarketDataSink
    {
    public:
        virtual ~IMarketDataSink() = default;
        virtual void on_level_update(const Level2Update &update) = 0;
    };

    class MarketDataPublisher final : public api::IEventBatchSink
    {
    public:
        explicit MarketDataPublisher(IMarketDataSink &sink)
            : sink_(sink)
        {
        }

        void on_event(const api::EngineEvent &event) override;
        void on_events(const std::vector<api::EngineEvent> &events) override;

        [[nodiscard]] std::uint64_t next_sequence() const
        {
            return next_sequence_;
        }

    private:
        struct OrderKey
        {
            domain::InstrumentId instrument_id{};
            api::OrderId order_id{};

            friend bool operator==(OrderKey left, OrderKey right)
            {
                return left.instrument_id == right.instrument_id &&
                       left.order_id == right.order_id;
            }
        };

        struct OrderKeyHash
        {
            std::size_t operator()(OrderKey key) const
            {
                return static_cast<std::size_t>(key.order_id) ^
                    (static_cast<std::size_t>(key.instrument_id) << 1U);
            }
        };

        struct TrackedOrder
        {
            api::Side side{};
            domain::Price price{0};
            domain::Quantity remaining_quantity{0};
        };

        struct LevelKey
        {
            domain::InstrumentId instrument_id{};
            api::Side side{};
            domain::Price price{0};

            friend bool operator<(LevelKey left, LevelKey right)
            {
                if (left.instrument_id != right.instrument_id)
                {
                    return left.instrument_id < right.instrument_id;
                }
                if (left.side != right.side)
                {
                    return left.side == api::Side::buy;
                }
                return left.price < right.price;
            }
        };

        void add_order(const api::OrderAccepted &accepted);
        void apply_trade(const api::TradeExecuted &trade);
        void remove_order(const api::OrderCanceled &canceled);
        void emit_level(
            domain::InstrumentId instrument_id, api::Side side, domain::Price price);
        void flush_pending_levels();
        [[nodiscard]] LevelKey level_key(
            domain::InstrumentId instrument_id, api::Side side, domain::Price price) const;

        IMarketDataSink &sink_;
        std::uint64_t next_sequence_{1};
        std::unordered_map<OrderKey, TrackedOrder, OrderKeyHash> orders_;
        std::map<LevelKey, api::Quantity> level_quantities_;
        bool batching_{false};
        std::map<LevelKey, bool> pending_levels_;
    };

} // namespace exchange_core::market_data
