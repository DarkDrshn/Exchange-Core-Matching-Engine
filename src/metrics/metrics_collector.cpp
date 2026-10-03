#include "exchange_core/metrics/metrics_collector.hpp"

#include <type_traits>
#include <variant>

namespace exchange_core::metrics
{
    void MetricsCollector::on_event(const api::EngineEvent &event)
    {
        on_events({event});
    }

    void MetricsCollector::on_events(const std::vector<api::EngineEvent> &events)
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events_seen_ += static_cast<std::uint64_t>(events.size());

        for (const auto &event : events)
        {
            std::visit(
                [this](const auto &typed_event)
                {
                    using EventType = std::decay_t<decltype(typed_event)>;
                    if constexpr (std::is_same_v<EventType, api::OrderAccepted>)
                    {
                        ++accepted_orders_;
                    }
                    else if constexpr (std::is_same_v<EventType, api::TradeExecuted>)
                    {
                        ++trades_executed_;
                    }
                    else if constexpr (std::is_same_v<EventType, api::OrderCanceled>)
                    {
                        ++canceled_orders_;
                    }
                    else if constexpr (std::is_same_v<EventType, api::OrderRejected>)
                    {
                        ++rejected_orders_;
                    }
                },
                event);
        }
    }

    void MetricsCollector::record_latency(std::chrono::nanoseconds duration)
    {
        const auto nanoseconds = static_cast<std::uint64_t>(duration.count());
        std::lock_guard<std::mutex> lock(mutex_);
        ++command_latency_samples_;
        command_latency_ns_total_ += nanoseconds;
        if (nanoseconds > command_latency_ns_max_)
        {
            command_latency_ns_max_ = nanoseconds;
        }
    }

    void MetricsCollector::reset()
    {
        std::lock_guard<std::mutex> lock(mutex_);
        events_seen_ = 0;
        accepted_orders_ = 0;
        rejected_orders_ = 0;
        trades_executed_ = 0;
        canceled_orders_ = 0;
        command_latency_samples_ = 0;
        command_latency_ns_total_ = 0;
        command_latency_ns_max_ = 0;
    }

    MetricsSnapshot MetricsCollector::snapshot() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return MetricsSnapshot{
            events_seen_,
            accepted_orders_,
            rejected_orders_,
            trades_executed_,
            canceled_orders_,
            command_latency_samples_,
            command_latency_ns_total_,
            command_latency_ns_max_};
    }

} // namespace exchange_core::metrics
