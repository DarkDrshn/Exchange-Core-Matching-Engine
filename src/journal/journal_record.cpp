#include "exchange_core/journal/journal_record.hpp"

#include <sstream>

namespace exchange_core::journal
{
    std::string to_string(JournalRecordKind kind)
    {
        switch (kind)
        {
            case JournalRecordKind::order_accepted:
                return "order_accepted";
            case JournalRecordKind::order_rejected:
                return "order_rejected";
            case JournalRecordKind::trade_executed:
                return "trade_executed";
            case JournalRecordKind::order_canceled:
                return "order_canceled";
            case JournalRecordKind::lifecycle_event:
                return "lifecycle_event";
        }
        return "unknown";
    }

    std::string serialize_record(const JournalRecord &record)
    {
        std::ostringstream output;
        output << "version=" << record.version
               << " kind=" << to_string(record.kind)
               << " sequence=" << record.sequence
               << " instrument_id=" << record.instrument_id
               << " order_id=" << record.order_id
               << " side=" << (record.side == api::Side::buy ? "buy" : "sell")
               << " price=" << record.price
               << " quantity=" << record.quantity
               << " remaining_quantity=" << record.remaining_quantity
               << " execution_quantity=" << record.execution_quantity
               << " incoming_order_id=" << record.incoming_order_id
               << " resting_order_id=" << record.resting_order_id
               << " account_id=" << record.account_id
               << " reason=" << record.reason;
        return output.str();
    }

} // namespace exchange_core::journal
