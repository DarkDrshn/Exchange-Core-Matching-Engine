#pragma once

#include "exchange_core/api/events.hpp"
#include "exchange_core/engine/matching_engine.hpp"

#include <cstdint>
#include <unordered_map>

namespace exchange_core::gateway
{
    using ClientId = std::uint64_t;
    using RequestSequence = std::uint64_t;

    struct PlaceOrderCommand
    {
        ClientId client_id{};
        RequestSequence sequence{};
        api::PlaceOrder order{};
    };

    struct CancelOrderCommand
    {
        ClientId client_id{};
        RequestSequence sequence{};
        api::CancelOrder cancel{};
    };

    class OrderGateway
    {
    public:
        explicit OrderGateway(engine::MatchingEngine &matching_engine)
            : matching_engine_(matching_engine)
        {
        }

        bool register_client(ClientId client_id, api::AccountId account_id);
        bool unregister_client(ClientId client_id);

        [[nodiscard]] engine::MatchingEngine::EventBatch place_order(
            const PlaceOrderCommand &command);
        [[nodiscard]] engine::MatchingEngine::EventBatch cancel_order(
            const CancelOrderCommand &command);

        [[nodiscard]] RequestSequence next_sequence(ClientId client_id) const;

    private:
        struct ClientSession
        {
            api::AccountId account_id{};
            RequestSequence next_sequence{1};
        };

        [[nodiscard]] engine::MatchingEngine::EventBatch reject(
            const api::PlaceOrder &order, api::RejectReason reason) const;
        [[nodiscard]] engine::MatchingEngine::EventBatch reject(
            const api::CancelOrder &cancel, api::RejectReason reason) const;
        [[nodiscard]] bool authorize(
            ClientId client_id, RequestSequence sequence, api::AccountId account_id);

        engine::MatchingEngine &matching_engine_;
        std::unordered_map<ClientId, ClientSession> clients_;
    };

} // namespace exchange_core::gateway
