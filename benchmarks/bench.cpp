#include "exchange_core/benchmark/benchmark.hpp"

#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
    std::size_t parse_iterations(int argc, char **argv)
    {
        if (argc <= 1)
        {
            return 50000U;
        }

        try
        {
            return static_cast<std::size_t>(std::stoull(argv[1]));
        }
        catch (const std::exception &)
        {
            return 50000U;
        }
    }
} // namespace

int main(int argc, char **argv)
{
    const auto iterations = parse_iterations(argc, argv);
    const auto result = exchange_core::benchmark::run_place_cancel_benchmark(iterations);
    std::cout << exchange_core::benchmark::format_benchmark_result(result) << '\n';
    return 0;
}
