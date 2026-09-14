#include "exchange_core/gateway/order_gateway.hpp"

namespace exchange_core::gateway
{
    bool OrderGateway::register_client(ClientId client_id, api::AccountId account_id)
    {
        if (client_id == 0 || account_id == 0 || clients_.find(client_id) != clients_.end())
        {
            return false;
        }
        clients_.emplace(client_id, ClientSession{account_id, 1});
        return true;
    }

    bool OrderGateway::unregister_client(ClientId client_id)
    {
        return clients_.erase(client_id) == 1;
    }

    bool OrderGateway::authorize(
        ClientId client_id, RequestSequence sequence, api::AccountId account_id)
    {
        const auto client = clients_.find(client_id);
        if (client == clients_.end() || client->second.account_id != account_id ||
            sequence != client->second.next_sequence)
        {
            return false;
        }
        ++client->second.next_sequence;
        return true;
    }

    engine::MatchingEngine::EventBatch OrderGateway::reject(
        const api::PlaceOrder &order, api::RejectReason reason) const
    {
        const domain::Order rejected_order{
            order.instrument_id,
            order.order_id,
            order.side,
            domain::Price{order.price},
            domain::Quantity{order.quantity},
            domain::Quantity{order.quantity},
            domain::OrderStatus::rejected,
            order.order_type,
            order.account_id};
        return {api::OrderRejected{
            rejected_order, order.instrument_id, order.order_id, reason}};
    }

    engine::MatchingEngine::EventBatch OrderGateway::reject(
        const api::CancelOrder &cancel, api::RejectReason reason) const
    {
        const domain::Order rejected_order{
            cancel.instrument_id,
            cancel.order_id,
            api::Side::buy,
            domain::Price{0},
            domain::Quantity{0},
            domain::Quantity{0},
            domain::OrderStatus::rejected,
            api::OrderType::limit,
            cancel.account_id};
        return {api::OrderRejected{
            rejected_order, cancel.instrument_id, cancel.order_id, reason}};
    }

    engine::MatchingEngine::EventBatch OrderGateway::place_order(
        const PlaceOrderCommand &command)
    {
        const auto client = clients_.find(command.client_id);
        if (client == clients_.end())
        {
            return reject(command.order, api::RejectReason::unknown_client);
        }
        if (client->second.account_id != command.order.account_id)
        {
            return reject(command.order, api::RejectReason::unauthorized_order);
        }
        if (command.sequence != client->second.next_sequence)
        {
            return reject(command.order, api::RejectReason::invalid_request_sequence);
        }
        ++client->second.next_sequence;
        return matching_engine_.place_order(command.order);
    }

    engine::MatchingEngine::EventBatch OrderGateway::cancel_order(
        const CancelOrderCommand &command)
    {
        const auto client = clients_.find(command.client_id);
        if (client == clients_.end())
        {
            return reject(command.cancel, api::RejectReason::unknown_client);
        }
        if (client->second.account_id != command.cancel.account_id)
        {
            return reject(command.cancel, api::RejectReason::unauthorized_order);
        }
        if (command.sequence != client->second.next_sequence)
        {
            return reject(command.cancel, api::RejectReason::invalid_request_sequence);
        }
        ++client->second.next_sequence;
        return matching_engine_.cancel_order(command.cancel);
    }

    RequestSequence OrderGateway::next_sequence(ClientId client_id) const
    {
        const auto client = clients_.find(client_id);
        return client == clients_.end() ? 0 : client->second.next_sequence;
    }

} // namespace exchange_core::gateway
