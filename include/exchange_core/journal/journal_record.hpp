#pragma once

#include "exchange_core/api/order_types.hpp"

#include <cstdint>
#include <string>

namespace exchange_core::journal
{
    enum class JournalRecordKind
    {
        order_accepted,
        order_rejected,
        trade_executed,
        order_canceled,
        lifecycle_event,
    };

    struct JournalRecord
    {
        std::uint64_t version{1};
        JournalRecordKind kind{JournalRecordKind::order_accepted};
        std::uint64_t sequence{0};
        std::uint64_t instrument_id{0};
        std::uint64_t order_id{0};
        api::Side side{api::Side::buy};
        api::Price price{0};
        api::Quantity quantity{0};
        api::Quantity remaining_quantity{0};
        api::Quantity execution_quantity{0};
        std::uint64_t incoming_order_id{0};
        std::uint64_t resting_order_id{0};
        std::uint64_t account_id{0};
        std::uint64_t reason{0};
    };

    [[nodiscard]] std::string to_string(JournalRecordKind kind);
    [[nodiscard]] std::string serialize_record(const JournalRecord &record);

} // namespace exchange_core::journal
