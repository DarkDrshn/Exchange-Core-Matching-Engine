#include "exchange_core/benchmark/benchmark.hpp"

#include "exchange_core/engine/matching_engine.hpp"

#include <chrono>
#include <sstream>

namespace exchange_core::benchmark
{
    BenchmarkResult run_place_cancel_benchmark(std::size_t iterations)
    {
        BenchmarkResult result{};
        if (iterations == 0)
        {
            return result;
        }

        engine::MatchingEngine engine;
        const auto instrument_registered = engine.register_instrument({
            1,
            "PRIMARY",
            exchange_core::domain::Price{1},
            exchange_core::domain::Quantity{1}});
        if (!instrument_registered)
        {
            return result;
        }

        const auto start = std::chrono::steady_clock::now();
        for (std::size_t index = 0; index < iterations; ++index)
        {
            const auto order_id = static_cast<api::OrderId>(1000 + index);
            const auto accepted = engine.place_order(
                api::PlaceOrder{1, order_id, api::Side::buy, 100, 10,
                    api::OrderType::limit, 200 + static_cast<api::AccountId>(index)});
            if (std::holds_alternative<api::OrderAccepted>(accepted.front()))
            {
                ++result.accepted_orders;
            }

            const auto canceled = engine.cancel_order(
                api::CancelOrder{1, order_id, 200 + static_cast<api::AccountId>(index)});
            if (std::holds_alternative<api::OrderCanceled>(canceled.front()))
            {
                ++result.canceled_orders;
            }
        }

        const auto end = std::chrono::steady_clock::now();
        result.iterations = iterations;
        result.operations = iterations * 2U;
        result.elapsed_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count());
        if (result.elapsed_ns > 0U)
        {
            result.ops_per_second = static_cast<double>(result.operations) * 1.0e9 /
                static_cast<double>(result.elapsed_ns);
        }
        else
        {
            result.ops_per_second = 0.0;
        }

        return result;
    }

    std::string format_benchmark_result(const BenchmarkResult &result)
    {
        std::ostringstream output;
        output << "iterations=" << result.iterations
               << " actions=" << result.operations
               << " accepted=" << result.accepted_orders
               << " canceled=" << result.canceled_orders
               << " elapsed_ns=" << result.elapsed_ns
               << " ops_per_second=" << result.ops_per_second;
        return output.str();
    }

} // namespace exchange_core::benchmark
