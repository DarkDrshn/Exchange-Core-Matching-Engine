#pragma once

#include "exchange_core/journal/journal_record.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace exchange_core::journal
{
    enum class JournalReadStatus
    {
        ok,
        malformed_record,
        io_error,
        invalid_version,
    };

    struct JournalReadResult
    {
        JournalReadStatus status{JournalReadStatus::ok};
        std::vector<JournalRecord> records;
        std::string error_message;

        [[nodiscard]] bool ok() const
        {
            return status == JournalReadStatus::ok;
        }
    };

    class FileJournal final
    {
    public:
        explicit FileJournal(std::string path);

        [[nodiscard]] const std::string &path() const;
        bool append_record(const JournalRecord &record);
        bool append_records(const std::vector<JournalRecord> &records);
        [[nodiscard]] JournalReadResult read_all() const;
        bool truncate();

    private:
        std::string path_;
    };

} // namespace exchange_core::journal
