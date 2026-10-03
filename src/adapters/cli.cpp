#include "exchange_core/benchmark/benchmark.hpp"
#include "exchange_core/engine/matching_engine.hpp"
#include "exchange_core/metrics/metrics_collector.hpp"

#include <iostream>
#include <string>
#include <vector>

namespace
{
    void print_usage()
    {
        std::cout << "Usage:\n"
                  << "  exchange_core_cli --benchmark [iterations]\n"
                  << "  exchange_core_cli --stats\n"
                  << "  exchange_core_cli --simulate\n"
                  << "  exchange_core_cli --help\n";
    }

    std::size_t parse_iterations(const std::string &value, std::size_t fallback)
    {
        try
        {
            return static_cast<std::size_t>(std::stoull(value));
        }
        catch (const std::exception &)
        {
            return fallback;
        }
    }

    void print_metrics_snapshot()
    {
        exchange_core::metrics::MetricsCollector metrics;
        exchange_core::engine::MatchingEngine engine({}, &metrics);
        engine.register_instrument({
            1,
            "PRIMARY",
            exchange_core::domain::Price{1},
            exchange_core::domain::Quantity{1}});

        const auto bid = engine.place_order(
            exchange_core::api::PlaceOrder{1, 1001, exchange_core::api::Side::buy,
                100, 10, exchange_core::api::OrderType::limit, 10});
        const auto ask = engine.place_order(
            exchange_core::api::PlaceOrder{1, 1002, exchange_core::api::Side::sell,
                100, 10, exchange_core::api::OrderType::limit, 11});

        const auto snapshot = metrics.snapshot();
        std::cout << "events_seen=" << snapshot.events_seen
                  << " accepted_orders=" << snapshot.accepted_orders
                  << " rejected_orders=" << snapshot.rejected_orders
                  << " trades_executed=" << snapshot.trades_executed
                  << " canceled_orders=" << snapshot.canceled_orders
                  << " benchmark_probe=" << (bid.empty() ? 0U : 1U) + (ask.empty() ? 0U : 1U)
                  << '\n';
    }

    void print_simulation()
    {
        exchange_core::engine::MatchingEngine engine;
        engine.register_instrument({
            1,
            "PRIMARY",
            exchange_core::domain::Price{1},
            exchange_core::domain::Quantity{1}});

        engine.place_order({1, 2001, exchange_core::api::Side::buy, 100, 8,
            exchange_core::api::OrderType::limit, 30});
        engine.place_order({1, 2002, exchange_core::api::Side::sell, 100, 8,
            exchange_core::api::OrderType::limit, 31});

        std::cout << "simulation=trade_match\n";
    }
} // namespace

int main(int argc, char **argv)
{
    const std::vector<std::string> arguments(argv + 1, argv + argc);
    if (arguments.empty() || arguments.front() == "--help")
    {
        print_usage();
        return 0;
    }

    const std::string option = arguments.front();
    if (option == "--benchmark")
    {
        const auto iterations = arguments.size() > 1
            ? parse_iterations(arguments[1], 50000U)
            : 50000U;
        const auto result = exchange_core::benchmark::run_place_cancel_benchmark(iterations);
        std::cout << exchange_core::benchmark::format_benchmark_result(result) << '\n';
        return 0;
    }

    if (option == "--stats")
    {
        print_metrics_snapshot();
        return 0;
    }

    if (option == "--simulate")
    {
        print_simulation();
        return 0;
    }

    std::cerr << "Unknown option: " << option << '\n';
    print_usage();
    return 2;
}
