#include "exchange_core/risk/account_risk_manager.hpp"

#include <algorithm>
#include <limits>
#include <variant>

namespace exchange_core::risk
{
    namespace
    {
        constexpr api::Quantity basis_points_scale{10000};

        api::Quantity saturated_multiply(api::Quantity left, api::Quantity right)
        {
            if (left == api::Quantity{0} || right == api::Quantity{0})
            {
                return 0;
            }
            const auto maximum = std::numeric_limits<api::Quantity>::max();
            return left > maximum / right ? maximum : left * right;
        }

        api::Quantity proportional_release(
            api::Quantity reserved, api::Quantity released, api::Quantity remaining)
        {
            const auto whole_units = reserved / remaining;
            const auto remainder = reserved % remaining;
            return saturated_multiply(whole_units, released) +
                saturated_multiply(remainder, released) / remaining;
        }
    }

    AccountRiskManager::AccountState AccountRiskManager::state_for(
        api::AccountId account_id, domain::InstrumentId instrument_id) const
    {
        const auto account = account_states_.find(account_id);
        if (account == account_states_.end())
        {
            return {};
        }
        const auto instrument = account->second.find(instrument_id);
        return instrument == account->second.end() ? AccountState{} : instrument->second;
    }

    AccountRejectReason AccountRiskManager::evaluate(const api::PlaceOrder &request) const
    {
        const auto state = state_for(request.account_id, request.instrument_id);
        if (request.quantity > configuration_.maximum_open_order_quantity ||
            state.open_order_quantity >
                configuration_.maximum_open_order_quantity - request.quantity)
        {
            return AccountRejectReason::open_order_limit;
        }

        const auto position = state.position;
        if (request.quantity > static_cast<api::Quantity>(
            std::numeric_limits<std::int64_t>::max()))
        {
            return AccountRejectReason::position_limit;
        }
        const auto quantity = static_cast<std::int64_t>(request.quantity);
        const auto configured_position_limit = configuration_.maximum_position_quantity >
                static_cast<api::Quantity>(std::numeric_limits<std::int64_t>::max())
            ? std::numeric_limits<std::int64_t>::max()
            : static_cast<std::int64_t>(configuration_.maximum_position_quantity);
        const auto buy_reserved = static_cast<std::int64_t>(state.buy_reserved_quantity);
        const auto sell_reserved = static_cast<std::int64_t>(state.sell_reserved_quantity);
        const auto projected_buy_position = position + buy_reserved +
            (request.side == api::Side::buy ? quantity : 0);
        const auto projected_sell_position = position - sell_reserved -
            (request.side == api::Side::sell ? quantity : 0);
        if (projected_buy_position > configured_position_limit ||
            projected_sell_position < -configured_position_limit)
        {
            return AccountRejectReason::position_limit;
        }

        const auto state_margin = state.reserved_margin;
        const auto order_margin = margin_for(request);
        if (order_margin > configuration_.maximum_account_credit ||
            state_margin > configuration_.maximum_account_credit - order_margin)
        {
            return AccountRejectReason::credit_limit;
        }

        return AccountRejectReason::none;
    }

    api::Quantity AccountRiskManager::margin_for(const api::PlaceOrder &request) const
    {
        const auto notional = request.order_type == api::OrderType::market
            ? configuration_.maximum_order_notional
            : saturated_multiply(static_cast<api::Quantity>(request.price), request.quantity);
        return saturated_multiply(notional, configuration_.initial_margin_basis_points) /
            basis_points_scale;
    }

    void AccountRiskManager::reserve(const api::PlaceOrder &request)
    {
        const OrderKey key{request.instrument_id, request.order_id};
        const auto reserved_order_margin = margin_for(request);
        reservations_[key] = Reservation{
            request.account_id, request.side, request.quantity, reserved_order_margin};
        auto &state = account_states_[request.account_id][request.instrument_id];
        state.open_order_quantity += request.quantity;
        state.reserved_margin += reserved_order_margin;
        if (request.side == api::Side::buy)
        {
            state.buy_reserved_quantity += request.quantity;
        }
        else
        {
            state.sell_reserved_quantity += request.quantity;
        }
    }

    void AccountRiskManager::release(OrderKey key, api::Quantity quantity)
    {
        const auto reservation = reservations_.find(key);
        if (reservation == reservations_.end())
        {
            return;
        }

        const auto released_quantity = std::min(quantity, reservation->second.remaining_quantity);
        auto &state = account_states_[reservation->second.account_id][key.instrument_id];
        state.open_order_quantity -= released_quantity;
        if (reservation->second.side == api::Side::buy)
        {
            state.buy_reserved_quantity -= released_quantity;
        }
        else
        {
            state.sell_reserved_quantity -= released_quantity;
        }
        const auto released_margin = released_quantity == reservation->second.remaining_quantity
            ? reservation->second.reserved_margin
            : proportional_release(
                reservation->second.reserved_margin,
                released_quantity,
                reservation->second.remaining_quantity);
        state.reserved_margin -= released_margin;
        reservation->second.reserved_margin -= released_margin;
        reservation->second.remaining_quantity -= released_quantity;
        if (reservation->second.remaining_quantity == 0)
        {
            reservations_.erase(reservation);
        }
    }

    void AccountRiskManager::update_position(
        api::AccountId account_id, domain::InstrumentId instrument_id,
        api::Side side, api::Quantity quantity)
    {
        auto &position = account_states_[account_id][instrument_id].position;
        const auto signed_quantity = static_cast<std::int64_t>(quantity);
        if (side == api::Side::buy)
        {
            position += signed_quantity;
        }
        else
        {
            position -= signed_quantity;
        }
    }

    void AccountRiskManager::apply(
        const api::PlaceOrder &request,
        const std::vector<api::EngineEvent> &events,
        bool reservation_created)
    {
        const OrderKey incoming_key{request.instrument_id, request.order_id};
        for (const auto &event : events)
        {
            if (const auto *trade = std::get_if<api::TradeExecuted>(&event))
            {
                if (reservation_created)
                {
                    release(incoming_key, trade->execution_quantity);
                }
                update_position(request.account_id, request.instrument_id,
                    request.side, trade->execution_quantity);

                const OrderKey resting_key{
                    trade->instrument_id, trade->resting_order_id};
                const auto resting = reservations_.find(resting_key);
                if (resting != reservations_.end())
                {
                    const auto resting_side = resting->second.side;
                    const auto resting_account = resting->second.account_id;
                    release(resting_key, trade->execution_quantity);
                    update_position(resting_account, trade->instrument_id,
                        resting_side, trade->execution_quantity);
                }
            }
            else if (const auto *canceled = std::get_if<api::OrderCanceled>(&event))
            {
                release({canceled->instrument_id, canceled->order_id},
                    std::numeric_limits<api::Quantity>::max());
            }
            else if (std::holds_alternative<api::OrderRejected>(event))
            {
                if (reservation_created)
                {
                    release(incoming_key, request.quantity);
                }
            }
        }

        if (reservation_created &&
            (request.order_type == api::OrderType::ioc ||
            request.order_type == api::OrderType::market ||
            request.order_type == api::OrderType::fok))
        {
            release(incoming_key, request.quantity);
        }
    }

    std::int64_t AccountRiskManager::position(
        api::AccountId account_id, domain::InstrumentId instrument_id) const
    {
        return state_for(account_id, instrument_id).position;
    }

    api::Quantity AccountRiskManager::open_order_quantity(api::AccountId account_id) const
    {
        api::Quantity total{};
        const auto account = account_states_.find(account_id);
        if (account == account_states_.end())
        {
            return total;
        }
        for (const auto &entry : account->second)
        {
            total += entry.second.open_order_quantity;
        }
        return total;
    }

    api::Quantity AccountRiskManager::reserved_margin(api::AccountId account_id) const
    {
        api::Quantity total{};
        const auto account = account_states_.find(account_id);
        if (account == account_states_.end())
        {
            return total;
        }
        for (const auto &entry : account->second)
        {
            total += entry.second.reserved_margin;
        }
        return total;
    }

} // namespace exchange_core::risk