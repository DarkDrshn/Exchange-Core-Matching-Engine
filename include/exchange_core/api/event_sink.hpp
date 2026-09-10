#pragma once

#include "exchange_core/api/events.hpp"

namespace exchange_core::api
{
    class IEventSink
    {
    public:
        virtual ~IEventSink() = default;
        virtual void on_event(const EngineEvent &event) = 0;
    };
}
