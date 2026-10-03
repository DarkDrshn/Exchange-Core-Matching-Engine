#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace exchange_core::benchmark
{
    struct BenchmarkResult
    {
        std::size_t iterations{0};
        std::size_t operations{0};
        std::uint64_t elapsed_ns{0};
        double ops_per_second{0.0};
        std::size_t accepted_orders{0};
        std::size_t canceled_orders{0};
    };

    [[nodiscard]] BenchmarkResult run_place_cancel_benchmark(std::size_t iterations = 50000);
    [[nodiscard]] std::string format_benchmark_result(const BenchmarkResult &result);

} // namespace exchange_core::benchmark
