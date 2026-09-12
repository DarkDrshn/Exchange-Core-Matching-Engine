#pragma once

#include "exchange_core/api/events.hpp"
#include "exchange_core/api/order_types.hpp"
#include "exchange_core/engine/engine_config.hpp"

#include <cstdint>
#include <vector>
#include <unordered_map>

namespace exchange_core::risk
{
    enum class AccountRejectReason
    {
        none,
        position_limit,
        open_order_limit,
    };

    class AccountRiskManager
    {
    public:
        explicit AccountRiskManager(engine::EngineConfig configuration)
            : configuration_(configuration)
        {
        }

        [[nodiscard]] AccountRejectReason evaluate(const api::PlaceOrder &request) const;
        void reserve(const api::PlaceOrder &request);
        void apply(const api::PlaceOrder &request, const std::vector<api::EngineEvent> &events);

        [[nodiscard]] std::int64_t position(
            api::AccountId account_id, domain::InstrumentId instrument_id) const;
        [[nodiscard]] api::Quantity open_order_quantity(api::AccountId account_id) const;

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

        struct Reservation
        {
            api::AccountId account_id{};
            api::Side side{};
            api::Quantity remaining_quantity{};
        };

        struct AccountState
        {
            std::int64_t position{};
            api::Quantity open_order_quantity{};
            api::Quantity buy_reserved_quantity{};
            api::Quantity sell_reserved_quantity{};
        };

        [[nodiscard]] AccountState state_for(
            api::AccountId account_id, domain::InstrumentId instrument_id) const;
        void update_position(
            api::AccountId account_id, domain::InstrumentId instrument_id,
            api::Side side, api::Quantity quantity);
        void release(OrderKey key, api::Quantity quantity);

        engine::EngineConfig configuration_;
        std::unordered_map<api::AccountId,
            std::unordered_map<domain::InstrumentId, AccountState>> account_states_;
        std::unordered_map<OrderKey, Reservation, OrderKeyHash> reservations_;
    };

} // namespace exchange_core::risk
