#include "exchange_core/snapshot/snapshot_store.hpp"

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string_view>

#if defined(__unix__) || defined(__APPLE__)
#include <fcntl.h>
#include <unistd.h>
#endif

namespace exchange_core::snapshot
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

        bool parse_u64(const std::string &text, std::uint64_t &value)
        {
            if (text.empty())
            {
                return false;
            }

            try
            {
                std::size_t consumed = 0;
                const auto parsed = std::stoull(text, &consumed);
                if (consumed != text.size())
                {
                    return false;
                }
                value = parsed;
                return true;
            }
            catch (const std::exception &)
            {
                return false;
            }
        }

        std::string parse_value(const std::string &line)
        {
            const auto equals = line.find('=');
            if (equals == std::string::npos)
            {
                return {};
            }
            return line.substr(equals + 1);
        }

        std::string build_temp_path(const std::string &target_path)
        {
            const std::filesystem::path target(target_path);
            const auto file_name = target.filename().string();
            const auto temp_name = file_name + ".tmp";
            return (target.parent_path() / temp_name).string();
        }

        bool sync_file_descriptor(const std::string &path)
        {
#if defined(__unix__) || defined(__APPLE__)
            const auto descriptor = ::open(path.c_str(), O_RDONLY);
            if (descriptor < 0)
            {
                return false;
            }
            const auto result = ::fsync(descriptor);
            ::close(descriptor);
            return result == 0;
#else
            (void)path;
            return true;
#endif
        }

        bool write_atomic(const std::string &target_path, const std::string &content)
        {
            const auto temp_path = build_temp_path(target_path);
            std::ofstream output(temp_path, std::ios::binary | std::ios::trunc);
            if (!output.is_open())
            {
                return false;
            }

            output << content;
            output.flush();
            if (!output.good())
            {
                std::error_code ignore;
                std::filesystem::remove(temp_path, ignore);
                return false;
            }

            if (!sync_file_descriptor(temp_path))
            {
                std::error_code ignore;
                std::filesystem::remove(temp_path, ignore);
                return false;
            }

            std::error_code rename_error;
            std::filesystem::rename(temp_path, target_path, rename_error);
            if (rename_error)
            {
                std::error_code ignore;
                std::filesystem::remove(temp_path, ignore);
                return false;
            }

            return true;
        }

    } // namespace

    SnapshotStore::SnapshotStore(std::string path)
        : path_(std::move(path))
    {
    }

    const std::string &SnapshotStore::path() const
    {
        return path_;
    }

    std::uint64_t SnapshotStore::checksum_for_payload(const std::string &payload)
    {
        constexpr std::uint64_t offset_basis = 14695981039346656037ULL;
        constexpr std::uint64_t prime = 1099511628211ULL;

        std::uint64_t hash = offset_basis;
        for (const char byte : payload)
        {
            const auto normalized = static_cast<unsigned char>(byte);
            hash ^= static_cast<std::uint64_t>(normalized);
            hash *= prime;
        }
        return hash;
    }

    std::string SnapshotStore::serialize_snapshot(const SnapshotRecord &record)
    {
        const auto checksum = checksum_for_payload(record.payload);
        std::ostringstream output;
        output << "version=" << record.version << '\n';
        output << "sequence=" << record.sequence << '\n';
        output << "payload_length=" << record.payload.size() << '\n';
        output << "checksum=" << checksum << '\n';
        output << record.payload;
        return output.str();
    }

    bool SnapshotStore::parse_snapshot_record(
        const std::string &serialized_snapshot,
        SnapshotRecord &record,
        std::string *error)
    {
        std::size_t position = 0U;
        auto read_line = [&serialized_snapshot, &position]() -> std::string
        {
            if (position >= serialized_snapshot.size())
            {
                return {};
            }

            const auto line_end = serialized_snapshot.find('\n', position);
            if (line_end == std::string::npos)
            {
                const auto line = serialized_snapshot.substr(position);
                position = serialized_snapshot.size();
                return line;
            }

            const auto line = serialized_snapshot.substr(position, line_end - position);
            position = line_end + 1U;
            return line;
        };

        std::string version_line = read_line();
        std::string sequence_line = read_line();
        std::string payload_length_line = read_line();
        std::string checksum_line = read_line();
        if (version_line.empty() || sequence_line.empty() || payload_length_line.empty() || checksum_line.empty())
        {
            if (error != nullptr)
            {
                *error = "missing snapshot header";
            }
            return false;
        }

        const auto version_value = trim(parse_value(version_line));
        const auto sequence_value = trim(parse_value(sequence_line));
        const auto payload_length_value = trim(parse_value(payload_length_line));
        const auto checksum_value = trim(parse_value(checksum_line));

        if (version_value.empty() || sequence_value.empty() || payload_length_value.empty() || checksum_value.empty())
        {
            if (error != nullptr)
            {
                *error = "malformed snapshot header";
            }
            return false;
        }

        std::uint64_t version = 0;
        std::uint64_t sequence = 0;
        std::uint64_t payload_length = 0;
        std::uint64_t checksum = 0;
        if (!parse_u64(version_value, version) || !parse_u64(sequence_value, sequence) ||
            !parse_u64(payload_length_value, payload_length) || !parse_u64(checksum_value, checksum))
        {
            if (error != nullptr)
            {
                *error = "invalid snapshot header field";
            }
            return false;
        }

        const auto payload_start = position;
        const auto payload_end = payload_start + static_cast<std::size_t>(payload_length);
        if (payload_end > serialized_snapshot.size())
        {
            if (error != nullptr)
            {
                *error = "snapshot payload length exceeded file size";
            }
            return false;
        }

        const std::string payload = serialized_snapshot.substr(payload_start, payload_length);
        const auto expected_checksum = checksum_for_payload(payload);
        if (expected_checksum != checksum)
        {
            if (error != nullptr)
            {
                *error = "snapshot checksum mismatch";
            }
            return false;
        }

        record = SnapshotRecord{version, sequence, payload, checksum};
        return true;
    }

    bool SnapshotStore::write(const SnapshotRecord &record) const
    {
        const SnapshotRecord normalized = record;
        const auto serialized = serialize_snapshot(normalized);
        return write_atomic(path_, serialized);
    }

    bool SnapshotStore::write(const std::string &payload, std::uint64_t sequence) const
    {
        return write(SnapshotRecord{1U, sequence, payload, 0U});
    }

    SnapshotReadResult SnapshotStore::read() const
    {
        SnapshotReadResult result{};
        std::ifstream input(path_, std::ios::binary | std::ios::in);
        if (!input.is_open())
        {
            result.status = SnapshotReadStatus::io_error;
            result.error_message = "unable to open snapshot file";
            return result;
        }

        std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        if (!input.good() && !input.eof())
        {
            result.status = SnapshotReadStatus::io_error;
            result.error_message = "failed while reading snapshot file";
            return result;
        }

        SnapshotRecord record{};
        std::string error; 
        if (!parse_snapshot_record(contents, record, &error))
        {
            result.status = error == "snapshot checksum mismatch" ? SnapshotReadStatus::checksum_mismatch
                : SnapshotReadStatus::malformed_snapshot;
            result.error_message = error.empty() ? "malformed snapshot" : error;
            return result;
        }

        result.status = SnapshotReadStatus::ok;
        result.record = record;
        return result;
    }

} // namespace exchange_core::snapshot
