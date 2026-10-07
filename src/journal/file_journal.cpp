#include "exchange_core/journal/file_journal.hpp"

#include <fstream>
#include <map>
#include <sstream>
#include <utility>

namespace exchange_core::journal
{
    namespace
    {
        std::string trim(const std::string &value)
        {
            const auto begin = value.find_first_not_of(" \t\r\n");
            if (begin == std::string::npos)
            {
                return {};
            }

            const auto end = value.find_last_not_of(" \t\r\n");
            return value.substr(begin, end - begin + 1U);
        }

        std::string lower_string(std::string value)
        {
            for (auto &character : value)
            {
                if (character >= 'A' && character <= 'Z')
                {
                    character = static_cast<char>(character - 'A' + 'a');
                }
            }
            return value;
        }

        bool parse_uint64(const std::string &value, std::uint64_t &result)
        {
            if (value.empty())
            {
                return false;
            }
            try
            {
                std::size_t consumed = 0;
                const auto parsed = std::stoull(value, &consumed);
                if (consumed != value.size())
                {
                    return false;
                }
                result = static_cast<std::uint64_t>(parsed);
                return true;
            }
            catch (const std::exception &)
            {
                return false;
            }
        }

        bool parse_price(const std::string &value, api::Price &result)
        {
            std::uint64_t parsed = 0;
            if (!parse_uint64(value, parsed))
            {
                return false;
            }
            result = static_cast<api::Price>(parsed);
            return true;
        }

        bool parse_quantity(const std::string &value, api::Quantity &result)
        {
            std::uint64_t parsed = 0;
            if (!parse_uint64(value, parsed))
            {
                return false;
            }
            result = static_cast<api::Quantity>(parsed);
            return true;
        }

        bool parse_side(const std::string &value, api::Side &side)
        {
            const auto normalized = lower_string(value);
            if (normalized == "buy")
            {
                side = api::Side::buy;
                return true;
            }
            if (normalized == "sell")
            {
                side = api::Side::sell;
                return true;
            }
            return false;
        }

        bool parse_kind(const std::string &value, JournalRecordKind &kind)
        {
            const auto normalized = lower_string(value);
            if (normalized == "order_accepted")
            {
                kind = JournalRecordKind::order_accepted;
                return true;
            }
            if (normalized == "order_rejected")
            {
                kind = JournalRecordKind::order_rejected;
                return true;
            }
            if (normalized == "trade_executed")
            {
                kind = JournalRecordKind::trade_executed;
                return true;
            }
            if (normalized == "order_canceled")
            {
                kind = JournalRecordKind::order_canceled;
                return true;
            }
            if (normalized == "lifecycle_event")
            {
                kind = JournalRecordKind::lifecycle_event;
                return true;
            }
            return false;
        }

        bool parse_record_line(const std::string &line, JournalRecord &record)
        {
            std::istringstream stream(line);
            std::string token;
            std::uint64_t version = 0;
            JournalRecordKind kind = JournalRecordKind::order_accepted;
            bool seen_version = false;
            bool seen_kind = false;
            bool seen_sequence = false;
            bool seen_instrument_id = false;
            bool seen_order_id = false;
            bool seen_side = false;
            bool seen_price = false;
            bool seen_quantity = false;
            bool seen_remaining_quantity = false;
            bool seen_execution_quantity = false;
            bool seen_incoming_order_id = false;
            bool seen_resting_order_id = false;
            bool seen_account_id = false;
            bool seen_reason = false;

            std::map<std::string, std::string> fields;
            while (stream >> token)
            {
                const auto equals = token.find('=');
                if (equals == std::string::npos)
                {
                    return false;
                }
                const auto key = token.substr(0, equals);
                const auto value = token.substr(equals + 1);
                fields[lower_string(key)] = value;
            }

            if (fields.empty())
            {
                return false;
            }

            for (const auto &[key, value] : fields)
            {
                if (key == "version")
                {
                    seen_version = parse_uint64(value, version);
                }
                else if (key == "kind")
                {
                    seen_kind = parse_kind(value, kind);
                }
                else if (key == "sequence")
                {
                    std::uint64_t parsed = 0;
                    seen_sequence = parse_uint64(value, parsed);
                    record.sequence = parsed;
                }
                else if (key == "instrument_id")
                {
                    std::uint64_t parsed = 0;
                    seen_instrument_id = parse_uint64(value, parsed);
                    record.instrument_id = parsed;
                }
                else if (key == "order_id")
                {
                    std::uint64_t parsed = 0;
                    seen_order_id = parse_uint64(value, parsed);
                    record.order_id = parsed;
                }
                else if (key == "side")
                {
                    seen_side = parse_side(value, record.side);
                }
                else if (key == "price")
                {
                    seen_price = parse_price(value, record.price);
                }
                else if (key == "quantity")
                {
                    seen_quantity = parse_quantity(value, record.quantity);
                }
                else if (key == "remaining_quantity")
                {
                    seen_remaining_quantity = parse_quantity(value, record.remaining_quantity);
                }
                else if (key == "execution_quantity")
                {
                    seen_execution_quantity = parse_quantity(value, record.execution_quantity);
                }
                else if (key == "incoming_order_id")
                {
                    std::uint64_t parsed = 0;
                    seen_incoming_order_id = parse_uint64(value, parsed);
                    record.incoming_order_id = parsed;
                }
                else if (key == "resting_order_id")
                {
                    std::uint64_t parsed = 0;
                    seen_resting_order_id = parse_uint64(value, parsed);
                    record.resting_order_id = parsed;
                }
                else if (key == "account_id")
                {
                    std::uint64_t parsed = 0;
                    seen_account_id = parse_uint64(value, parsed);
                    record.account_id = parsed;
                }
                else if (key == "reason")
                {
                    std::uint64_t parsed = 0;
                    seen_reason = parse_uint64(value, parsed);
                    record.reason = parsed;
                }
            }

            if (!seen_version || !seen_kind || !seen_sequence || !seen_instrument_id ||
                !seen_order_id || !seen_side || !seen_price || !seen_quantity ||
                !seen_remaining_quantity || !seen_execution_quantity ||
                !seen_incoming_order_id || !seen_resting_order_id || !seen_account_id ||
                !seen_reason)
            {
                return false;
            }

            if (version != 1U)
            {
                return false;
            }

            record.version = version;
            record.kind = kind;
            return true;
        }
    } // namespace

    FileJournal::FileJournal(std::string path)
        : path_(std::move(path))
    {
    }

    const std::string &FileJournal::path() const
    {
        return path_;
    }

    bool FileJournal::append_record(const JournalRecord &record)
    {
        std::ofstream output(path_, std::ios::app | std::ios::binary);
        if (!output.is_open())
        {
            return false;
        }

        output << serialize_record(record) << '\n';
        output.flush();
        return output.good();
    }

    bool FileJournal::append_records(const std::vector<JournalRecord> &records)
    {
        for (const auto &record : records)
        {
            if (!append_record(record))
            {
                return false;
            }
        }
        return true;
    }

    JournalReadResult FileJournal::read_all() const
    {
        JournalReadResult result{};
        std::ifstream input(path_, std::ios::in | std::ios::binary);
        if (!input.is_open())
        {
            result.status = JournalReadStatus::io_error;
            result.error_message = "unable to open journal file";
            return result;
        }

        std::string line;
        while (std::getline(input, line))
        {
            const auto trimmed = trim(line);
            if (trimmed.empty())
            {
                continue;
            }

            JournalRecord record{};
            if (!parse_record_line(trimmed, record))
            {
                result.status = JournalReadStatus::malformed_record;
                result.error_message = "malformed journal record";
                break;
            }

            result.records.push_back(record);
        }

        if (input.bad())
        {
            result.status = JournalReadStatus::io_error;
            result.error_message = "failed while reading journal file";
            result.records.clear();
        }

        if (result.status == JournalReadStatus::ok && result.records.empty())
        {
            return result;
        }

        return result;
    }

    bool FileJournal::truncate()
    {
        std::ofstream output(path_, std::ios::trunc | std::ios::binary);
        return output.good();
    }

} // namespace exchange_core::journal
