#pragma once

#include "exchange_core/api/events.hpp"

#include <vector>

namespace exchange_core::api
{
    class IEventSink
    {
    public:
        virtual ~IEventSink() = default;
        virtual void on_event(const EngineEvent &event) = 0;
    };

    class IEventBatchSink : public IEventSink
    {
    public:
        ~IEventBatchSink() override = default;
        virtual void on_events(const std::vector<EngineEvent> &events) = 0;
    };
}
