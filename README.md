# Exchange Core Matching Engine

A C++17 exchange core focused on deterministic price-time matching, clear module
boundaries, and efficient resource usage for HFT-oriented systems engineering.

This is an educational and research project. It is not production trading
infrastructure and does not connect to live exchanges or handle real funds.

## Features

- C++17 static library named `exchange_core`.
- Public API under `include/exchange_core/`.
- Private order-book state per `MatchingEngine` instance.
- Integer price and quantity types.
- Configurable maximum price and quantity limits.
- Buy and sell limit orders.
- Price-time priority matching.
- Partial fills and FIFO behavior at one price level.
- Order cancellation.
- Rejection of invalid and duplicate orders.
- Optional event-sink delivery after each completed state mutation.

The current implementation supports a single order book. The following features are
planned for future releases:

- Multiple instruments.
- Market, IOC, FOK, and post-only orders.
- Risk checks and position accounting.
- Market-data publication.
- Event journaling, snapshots, and replay.
- Metrics, CLI tooling, load testing, and GitHub Actions expansion.

Design notes and the longer-term roadmap are maintained in
[PROJECT_CONTEXT.md](PROJECT_CONTEXT.md). Project goals and engineering guidelines
are documented in [docs/PROJECT_CHARTER.md](docs/PROJECT_CHARTER.md).

## Repository Layout

```text
include/exchange_core/   Stable public headers and domain value types
src/domain/              Private order-book implementation
src/engine/               Matching-engine facade and configuration
tests/                   Compatibility tests
docs/                    Baselines and project documentation
CMakeLists.txt           Build and test configuration
```

The public headers are kept separate from the implementation so that the matching
engine can be used as a library without exposing its internal data structures.

## Build

Requirements:

- CMake 3.25 or newer
- C++17 compiler
- Make or Ninja

Configure and build from the repository root:

```bash
./scripts/build.sh debug
```

For a Release build, run `./scripts/build.sh release`. The script uses CMake
presets and stores generated files under `build/debug/` or `build/release/`.

The build enables `-Wall`, `-Wextra`, `-Wpedantic`, `-Wconversion`, and
`-Wsign-conversion` on GCC and Clang. Warnings are treated as errors by default.

Run the tests:

```bash
ctest --test-dir build --output-on-failure
```

The test suite currently covers the matching behavior listed above. A successful run
reports:

```text
100% tests passed, 0 tests failed
1/1 exchange_core_compatibility
```

The `build/` directory contains generated files and is excluded from Git.

## Public API Example

```cpp
#include "exchange_core/engine/matching_engine.hpp"

exchange_core::engine::MatchingEngine engine;

const auto events = engine.place_order({
    1001,
    exchange_core::api::Side::buy,
    100,
    10,
});
```

Orders return a batch of events. An optional `IEventSink` can receive the same events
after the complete book mutation has finished, so event consumers can safely call
back into the engine.

The engine accepts optional limits through `EngineConfig`:

```cpp
exchange_core::engine::MatchingEngine engine({100000, 1000});
```

Requests outside those limits are rejected before they reach the order book.

## Development Rules

- Keep matching behavior deterministic and testable.
- Use descriptive domain names and C++17-compatible interfaces.
- Do not use floating point for matching prices or quantities.
- Minimize allocations, copies, locks, logging, and other hot-path work.
- Record benchmark workload, compiler, build type, hardware, and latency statistics.
- Keep new modules within the folder boundaries described in `PROJECT_CONTEXT.md`.
- Keep Debug and Release builds warning-free.

## License

Voh toh nahi hai, but confidence hai 🫩