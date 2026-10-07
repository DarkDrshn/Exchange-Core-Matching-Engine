#pragma once

#include <cstdint>
#include <string>

namespace exchange_core::snapshot
{
    enum class SnapshotReadStatus
    {
        ok,
        io_error,
        malformed_snapshot,
        checksum_mismatch,
    };

    struct SnapshotRecord
    {
        std::uint64_t version{1};
        std::uint64_t sequence{0};
        std::string payload{};
        std::uint64_t checksum{0};
    };

    struct SnapshotReadResult
    {
        SnapshotReadStatus status{SnapshotReadStatus::ok};
        SnapshotRecord record{};
        std::string error_message{};
    };

    class SnapshotStore final
    {
    public:
        explicit SnapshotStore(std::string path);

        [[nodiscard]] const std::string &path() const;

        [[nodiscard]] bool write(const SnapshotRecord &record) const;
        [[nodiscard]] bool write(const std::string &payload, std::uint64_t sequence) const;
        [[nodiscard]] SnapshotReadResult read() const;

        [[nodiscard]] static std::uint64_t checksum_for_payload(const std::string &payload);
        [[nodiscard]] static std::string serialize_snapshot(const SnapshotRecord &record);
        [[nodiscard]] static bool parse_snapshot_record(
            const std::string &serialized_snapshot,
            SnapshotRecord &record,
            std::string *error = nullptr);

    private:
        std::string path_;
    };

} // namespace exchange_core::snapshot
