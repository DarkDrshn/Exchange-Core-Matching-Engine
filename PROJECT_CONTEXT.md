# Matching Engine Project Context

## Purpose

This document is the working index and implementation roadmap for turning the current
single-symbol C++17 matching engine into an original, maintainable personal project.
It records the architecture, module boundaries, testing contract, delivery sequence,
and future operating model in one place.

The current repository is a useful functional baseline. The target project should be
built by understanding the behavior, writing its own requirements, redesigning the
boundaries, and implementing the next version deliberately. Do not preserve copied
names, comments, demo scripts, or historical claims merely to make the project appear
original. Keep third-party attribution and repository history honest; originality
comes from the new design, implementation, tests, and engineering decisions.

## Current Baseline

Day 1 measurement details are recorded in [docs/baseline/BASELINE.md](docs/baseline/BASELINE.md).
Project goals and non-goals are recorded in [docs/PROJECT_CHARTER.md](docs/PROJECT_CHARTER.md).
The charter's Original Implementation Policy and HFT Engineering Standards apply to
every future module and pull request.

- C++17 static library named `engine`.
- One in-process `MatchingEngine` and one `OrderBook`.
- One implicit instrument.
- Limit orders only: buy, sell, place, cancel.
- Price-time priority with FIFO queues at each price level.
- Synchronous callbacks routed through raw client pointers.
- Catch2 unit tests for crossing, duplicate IDs, partial fills, FIFO, and cancel.
- A micro-benchmark for place/cancel operations.
- CMake build and a basic Ubuntu GitHub Actions workflow in the reference implementation.
- The new repository now contains the first namespaced `exchange_core` library and a
  local compatibility test target from Day 2.

Baseline verification on 2026-09-07: `ctest --test-dir build --output-on-failure`
passes with 1/1 tests.

Day 2 verification: the new repository configures with CMake, builds a C++17 static
library, and passes `exchange_core_compatibility` with 1/1 tests. The public API is
under `include/exchange_core/`; matching state is private to each `MatchingEngine`
instance and implementation code was written independently for this repository.

## Target Outcome

A deterministic, multi-instrument, event-driven matching engine that can be used as:

1. a reusable C++ library;
2. a command-line simulator and replay tool; and
3. a benchmarkable research project with reproducible CI artifacts.

The matching core remains single-writer per instrument for deterministic ordering. IO,
persistence, risk, metrics, and adapters stay outside the core so they can evolve
without changing price-time matching rules.

## Proposed Repository Structure

```text
exchange-core-matching-engine/
├── CMakeLists.txt
├── CMakePresets.json
├── LICENSE
├── README.md
├── PROJECT_CONTEXT.md
├── DESIGN.md
├── CHANGELOG.md
├── cmake/                         # reusable CMake helpers and warnings
├── include/matching_engine/
│   ├── api/                       # public requests, responses, callbacks
│   ├── domain/                    # Order, Trade, Instrument, value types
│   ├── engine/                    # MatchingEngine and engine configuration
│   ├── market_data/               # snapshots, L2 updates, subscriptions
│   ├── risk/                      # limits and pre-trade decisions
│   └── errors/                    # error codes and public error contract
├── src/
│   ├── api/
│   ├── domain/
│   ├── engine/
│   ├── market_data/
│   ├── risk/
│   ├── persistence/
│   ├── replay/
│   ├── metrics/
│   └── adapters/                  # CLI, file, and demo adapters
├── tests/
│   ├── unit/
│   ├── component/
│   ├── property/
│   ├── integration/
│   └── fixtures/
├── benchmarks/                    # latency, throughput, allocation workloads
├── tools/                         # replay, snapshot inspection, data generators
├── configs/                       # checked-in example configurations
├── scripts/                       # reproducible developer commands
├── docs/                          # protocols, decisions, performance reports
└── .github/workflows/
    ├── ci.yml
    ├── sanitizers.yml
    ├── static-analysis.yml
    └── benchmark.yml
```

### Placement Rules

- `include/matching_engine/` contains stable public headers only.
- `src/` contains implementation files and private helpers; do not expose internal
  containers or mutexes through public headers.
- `domain/` contains value objects and business invariants, not logging or file IO.
- `engine/` coordinates commands and matching; it does not own CLI concerns.
- `risk/` decides whether a request may enter the book; it must not mutate the book.
- `market_data/` observes committed engine events and publishes snapshots/deltas.
- `persistence/` writes journals and snapshots; it must not decide match outcomes.
- `replay/` consumes recorded events through the same command path used by tests.
- `metrics/` records counters and latency samples without changing behavior.
- `adapters/` translates external formats into the public API.
- Constants belong near their owning domain (`domain/` or a feature namespace). Only
  cross-cutting compile-time values belong in `config/` or a small `constants.hpp`.
- Generic utilities must be stateless and narrowly named. Do not create a catch-all
  `utils` folder for domain logic.
- Tests mirror production boundaries. A test should use the narrowest public surface
  that can prove its behavior.
- Write all implementation code from scratch for this repository. Study the reference
  only for behavior; do not copy its code, names, comments, tests, or folder layout.
- Use descriptive domain names and keep the hot path minimal: avoid unnecessary
  allocations, copies, locks, logging, IO, virtual dispatch, and unbounded state.
- Treat p50/p95/p99 latency, allocation count, memory footprint, throughput, and
  deterministic replay as required measurements for HFT-oriented changes.

## Modules to Add

1. **Instrument Registry**: symbols, tick size, lot size, status, and per-symbol books.
2. **Order Model and Types**: market, limit, IOC, FOK, post-only, and replace rules.
3. **Execution Reports**: execution IDs, cumulative quantity, remaining quantity, and
   explicit order lifecycle states.
4. **Risk Manager**: quantity, notional, position, credit, and fat-finger checks.
5. **Session Gateway**: client identity, request sequencing, authorization, and
   disconnect cleanup.
6. **Market Data Publisher**: consistent snapshots and incremental level-2 updates.
7. **Event Journal**: append-only commands, decisions, trades, and sequence numbers.
8. **Snapshot Store**: versioned snapshots with atomic write and checksum validation.
9. **Replay and Recovery**: rebuild state from snapshot plus journal and verify hashes.
10. **Metrics and Telemetry**: throughput, p50/p99 latency, rejects, depth, and fills.
11. **CLI and Configuration**: validated config, script runner, replay, inspection, and
    benchmark commands.
12. **Load Test Harness**: deterministic workload generation and latency reports.

## Architecture Rules

- Use integer fixed-point price and quantity types; never use floating point for
  matching decisions.
- Give every accepted command and emitted event a monotonically increasing sequence.
- Match one instrument in one deterministic command stream. Parallelize across symbols
  only after correctness tests prove isolation.
- Validate at the API boundary, then enforce domain invariants again in the engine.
- Publish events after state mutation through an event sink. Never invoke user code
  while the engine mutex is held.
- Replace raw callback ownership with an explicit sink/subscriber lifetime contract.
- Keep cancellation lookup O(1) through an order index; document any tradeoff in the
  order-level data structure.
- Make clocks injectable so tests and replay never depend on wall-clock time.
- Version serialized formats from the first journal and snapshot implementation.
- Treat allocation count, tail latency, and deterministic replay as first-class quality
  signals, not only average throughput.

## Delivery Estimate

A realistic solo implementation is **30 focused workdays (6 weeks)** at roughly 4-6
hours per day. A production-hardened version with external protocol adapters, durable
storage, deployment, and extensive performance tuning is closer to 8-10 weeks.
The schedule below deliberately keeps each day to one to three finishable tasks.

## Daily Implementation Plan

### Week 1: Baseline and Core Boundary

- [x] **Day 1**: Freeze a baseline benchmark and test report; write project goals and
  non-goals; create the `project-foundation` branch. See `docs/baseline/BASELINE.md`
  and `docs/PROJECT_CHARTER.md`.
- [x] **Day 2**: Add the `exchange_core` public namespace and library API around the
  personal project identity; preserve baseline behavior with compatibility tests.
- [ ] **Day 3**: Replace source globbing with explicit CMake source lists; add warnings,
  Debug/Release presets, and a clean out-of-tree build script.
- [ ] **Day 4**: Split protocol messages, domain value types, and engine configuration;
  add compile-time validation for price and quantity types.
- [ ] **Day 5**: Refactor callback delivery into an event sink that is invoked after the
  book mutation; add a regression test for re-entrant consumers.

### Week 2: Instruments and Order Semantics

- [ ] **Day 6**: Implement `InstrumentRegistry` and `InstrumentId`; route commands to
  independent books and test cross-symbol isolation.
- [ ] **Day 7**: Introduce explicit `Order`, `Trade`, `OrderStatus`, and `ExecutionId`
  models; remove duplicated lifecycle fields.
- [ ] **Day 8**: Implement IOC and post-only orders with rejection reasons and tests for
  every crossing and non-crossing path.
- [ ] **Day 9**: Implement market and FOK behavior with an explicit insufficient-liquidity
  policy; test that failed FOK orders leave no state change.
- [ ] **Day 10**: Implement cancel-replace semantics and sequence rules; add tests for
  priority loss, invalid replacement, and completed-order replacement.

### Week 3: Risk, Sessions, and Market Data

- [ ] **Day 11**: Add `RiskManager` interfaces and immutable limit configuration;
  implement quantity, notional, and price-band checks.
- [ ] **Day 12**: Add position and open-order accounting; test risk reservation and
  release on fills, cancels, and rejects.
- [ ] **Day 13**: Add `SessionGateway` with client identity, request IDs, duplicate
  request handling, and disconnect cleanup.
- [ ] **Day 14**: Define market-data event schemas and sequence guarantees; implement
  level-2 incremental updates from committed engine events.
- [ ] **Day 15**: Implement consistent book snapshots and a snapshot subscription API;
  add a consumer that reconstructs the book from deltas and compares hashes.

### Week 4: Durability and Recovery

- [ ] **Day 16**: Define versioned journal records for accepted commands, rejects,
  trades, cancels, and lifecycle events; document the format in `docs/`.
- [ ] **Day 17**: Implement an append-only file journal with flush/error handling and
  tests for truncation and malformed records.
- [ ] **Day 18**: Implement versioned snapshots with checksum, temporary-file write,
  fsync policy, and atomic rename.
- [ ] **Day 19**: Implement replay from an empty state and from snapshot-plus-journal;
  assert deterministic final state and sequence number.
- [ ] **Day 20**: Add crash-recovery integration tests, corruption rejection tests, and
  a CLI tool that inspects journal and snapshot metadata.

### Week 5: Quality, Performance, and Tooling

- [ ] **Day 21**: Add property tests for conservation of quantity, no crossed resting
  book, unique live IDs, and price-time priority.
- [ ] **Day 22**: Add randomized fuzz workloads for place, cancel, replace, and matching;
  store failing seeds as permanent fixtures.
- [ ] **Day 23**: Add ThreadSanitizer and Address/UndefinedBehaviorSanitizer workflows;
  fix all findings in the touched code.
- [ ] **Day 24**: Add clang-format verification, clang-tidy, compiler warnings-as-errors,
  and dependency/license checks.
- [ ] **Day 25**: Replace the old benchmark with reproducible workloads for resting,
  matching, cancel-heavy, multi-symbol, and replay paths.

### Week 6: Product Surface and CI Completion

- [ ] **Day 26**: Add a configuration parser and CLI commands for demo, benchmark,
  snapshot inspection, and replay; validate bad configurations cleanly.
- [ ] **Day 27**: Build `LoadTestHarness` with deterministic seeds, configurable clients,
  warmup, duration, and p50/p95/p99 latency output.
- [ ] **Day 28**: Add packaging/install targets, exported CMake targets, examples, and
  a concise README that describes the personal project and its design decisions.
- [ ] **Day 29**: Complete GitHub Actions: fast CI, sanitizers, static analysis, cached
  dependencies, coverage artifact, and benchmark artifact on manual or scheduled runs.
- [ ] **Day 30**: Run a clean-machine validation, review public headers and docs, record
  performance deltas, tag the first personal-project release, and update `CHANGELOG.md`.

## Testing Strategy

### Required Test Layers

- **Unit**: value validation, price-time matching, order types, risk rules, sequence
  handling, and serializers.
- **Component**: engine plus book, engine plus risk, event sink plus market data,
  journal plus replay, and snapshot plus restore.
- **Property**: conservation of quantities, no duplicate live IDs, no crossed resting
  prices, FIFO fairness, idempotent replay, and symbol isolation.
- **Integration**: complete order lifecycles through the public API, including session
  disconnects, persistence, and recovery.
- **Fuzz**: random valid and invalid command streams with retained seeds and a bounded
  runtime in CI.
- **Concurrency**: ThreadSanitizer runs, re-entrant event consumers, independent
  symbol streams, and shutdown while work is pending.
- **Performance**: report operations/sec plus p50, p95, p99, and max latency; compare
  against a checked-in baseline and avoid claiming hardware-independent results.

### Definition of Done for Every Module

1. Public contract and invariants are documented.
2. Unit and at least one component test exist.
3. Invalid input and failure behavior are covered.
4. No public header exposes a private implementation detail.
5. Debug, Release, sanitizer, and clean rebuild paths pass.
6. The module has one owner and no dependency cycle.

## GitHub Actions Plan

- **`ci.yml`**: Ubuntu matrix for GCC and Clang; configure with CMake presets; build,
  run CTest, upload test results, and cache CMake dependencies.
- **`sanitizers.yml`**: Debug AddressSanitizer, UndefinedBehaviorSanitizer, and
  ThreadSanitizer jobs; run unit, component, and deterministic fuzz smoke tests.
- **`static-analysis.yml`**: clang-format check, clang-tidy, compiler warnings as
  errors, CMake lint, and dependency/license verification.
- **`benchmark.yml`**: manually dispatched and scheduled; build Release, run fixed-seed
  workloads, upload JSON/CSV results, and never fail normal CI for machine variance.
- Pin action major versions, use least-privilege permissions, avoid secrets in pull
  requests, and make all workflow commands reproducible locally through `scripts/`.

## Milestones and Exit Criteria

- **M1 Foundation**: explicit architecture, stable public API, clean CMake, baseline
  tests, and no callback-under-lock behavior.
- **M2 Trading Semantics**: multiple instruments, order types, reports, and replacement
  rules with complete edge-case coverage.
- **M3 Operational Safety**: risk, sessions, market data, sequence numbers, and
  deterministic event delivery.
- **M4 Recovery**: journal, snapshots, replay, corruption handling, and recovery tests.
- **M5 Engineering Quality**: property/fuzz tests, sanitizer jobs, static analysis,
  reproducible benchmarks, CLI, packaging, and release documentation.

The project is ready for a first public personal-project release only when M5 passes
from a clean checkout and replay produces the same final state and event hash as the
original run.
