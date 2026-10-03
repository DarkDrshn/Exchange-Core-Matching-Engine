#pragma once

#include "exchange_core/api/event_sink.hpp"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <vector>

namespace exchange_core::metrics
{
    struct MetricsSnapshot
    {
        std::uint64_t events_seen{0};
        std::uint64_t accepted_orders{0};
        std::uint64_t rejected_orders{0};
        std::uint64_t trades_executed{0};
        std::uint64_t canceled_orders{0};
        std::uint64_t command_latency_samples{0};
        std::uint64_t command_latency_ns_total{0};
        std::uint64_t command_latency_ns_max{0};
    };

    class MetricsCollector final : public api::IEventBatchSink
    {
    public:
        void on_event(const api::EngineEvent &event) override;
        void on_events(const std::vector<api::EngineEvent> &events) override;

        void record_latency(std::chrono::nanoseconds duration);
        void reset();

        [[nodiscard]] MetricsSnapshot snapshot() const;

    private:
        mutable std::mutex mutex_;
        std::uint64_t events_seen_{0};
        std::uint64_t accepted_orders_{0};
        std::uint64_t rejected_orders_{0};
        std::uint64_t trades_executed_{0};
        std::uint64_t canceled_orders_{0};
        std::uint64_t command_latency_samples_{0};
        std::uint64_t command_latency_ns_total_{0};
        std::uint64_t command_latency_ns_max_{0};
    };

} // namespace exchange_core::metrics
