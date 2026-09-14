#include "exchange_core/risk/fat_finger_validator.hpp"

#include <limits>

namespace exchange_core::risk
{
    namespace
    {
        api::Price deviation_amount(api::Price reference_price, api::Quantity basis_points)
        {
            const auto reference = static_cast<api::Quantity>(reference_price);
            const auto whole_units = reference / 10000;
            const auto remainder = reference % 10000;
            const auto whole_component = whole_units * basis_points;
            const auto remainder_component = remainder * basis_points / 10000;
            return static_cast<api::Price>(whole_component + remainder_component);
        }
    }

    FatFingerResult FatFingerValidator::evaluate(
        const api::PlaceOrder &request, api::Price reference_price) const
    {
        if (request.order_type == api::OrderType::market)
        {
            return FatFingerResult::accepted;
        }
        if (reference_price <= 0)
        {
            return FatFingerResult::reference_price_unavailable;
        }

        const auto deviation = configuration_.maximum_reference_deviation_basis_points;
        const auto deviation_amount_value = deviation_amount(reference_price, deviation);
        const auto lower_bound = reference_price - deviation_amount_value;
        const auto maximum_price = std::numeric_limits<api::Price>::max();
        const auto upper_bound = deviation_amount_value > maximum_price - reference_price
            ? maximum_price
            : reference_price + deviation_amount_value;
        const auto price = request.price;

        return price >= lower_bound && price <= upper_bound
            ? FatFingerResult::accepted
            : FatFingerResult::outside_reference_band;
    }

} // namespace exchange_core::risk