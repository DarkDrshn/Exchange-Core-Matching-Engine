# Exchange Core Project Charter

## Vision

Build a deterministic C++ exchange core that demonstrates correct price-time
matching, explicit domain boundaries, operational safety, recoverability, and
reproducible performance measurement.

The project is intended to be a serious personal engineering project and learning
artifact. It is not presented as production trading infrastructure or as a copy of an
existing repository.

## Original Implementation Policy

All production code in this repository must be written from scratch for Exchange
Core. Do not copy and paste implementation code, variable names, class layouts,
comments, demo scenarios, or tests from the older reference project. The reference
implementation may be studied to understand behavior and used for comparison, but
each feature must be independently designed, named, implemented, and tested here.

Use descriptive names that communicate domain meaning: prefer `remaining_quantity`,
`price_level`, `instrument_id`, and `execution_sequence` over abbreviations or
single-letter variables. Keep functions focused, interfaces explicit, and ownership
visible through RAII, value types, and appropriate smart-pointer or non-owning-view
contracts. Avoid dead code, speculative abstractions, broad `utils` modules, hidden
global state, and comments that merely restate the code.

The folder structure in `PROJECT_CONTEXT.md` is part of the design. New code must be
placed in the module that owns its invariant, with stable public headers under
`include/matching_engine/`, implementation under `src/`, and tests mirroring the
production boundary. A new dependency or cross-module shortcut must be justified in
the design documentation.

## HFT Engineering Standards

This is an HFT-oriented systems project, so correctness and predictable low-latency
behavior guide implementation decisions:

- Keep the matching hot path small, deterministic, and free of logging, file IO, and
   avoidable virtual dispatch.
- Prefer fixed-size or preallocated data where the workload permits; minimize heap
   allocations, copies, lock contention, and cache-unfriendly pointer chasing.
- Use integer or fixed-point price and quantity types; never use floating point for
   order-book decisions.
- Keep ownership and lifetimes explicit. Do not use raw owning pointers or callbacks
   that execute while a matching lock is held.
- Measure p50, p95, p99, maximum latency, throughput, allocation count, and memory
   footprint. Do not optimize based on intuition or average throughput alone.
- Benchmark representative resting, matching, cancel, multi-instrument, and replay
   workloads with fixed seeds and documented hardware/compiler/build settings.
- Use profiling, sanitizers, compiler warnings, and disassembly when a performance
   decision matters. Preserve a readable implementation unless a measured result
   justifies added complexity.
- Optimize resource consumption as well as speed: bound queues, avoid unbounded maps,
   release completed-order state, and define overload/backpressure behavior.
- Every optimization must preserve price-time priority, deterministic replay, event
   sequencing, and failure safety. Correctness tests are a gate for performance work.

## Goals

1. Provide a reusable library for deterministic order matching.
2. Support multiple instruments without allowing state to leak between books.
3. Model order lifecycle, executions, rejects, and sequence numbers explicitly.
4. Add risk checks before orders reach the matching core.
5. Publish market-data events and consistent book snapshots.
6. Persist events, create snapshots, and recover through deterministic replay.
7. Make correctness visible through unit, component, property, fuzz, integration,
   sanitizer, and concurrency tests.
8. Make performance reproducible with fixed workloads and latency distributions.
9. Provide a small CLI for simulation, replay, inspection, and benchmarking.
10. Maintain a clean public C++ API with private implementation details hidden.

## Non-Goals

1. Connecting to a live exchange or placing real orders.
2. Handling real customer funds, custody, settlement, or regulatory compliance.
3. Claiming exchange-grade production latency or availability.
4. Implementing a distributed consensus system in the first release.
5. Using floating-point values for matching prices or quantities.
6. Building a full FIX, WebSocket, REST, or broker integration before the core is
   correct and tested.
7. Optimizing data structures before correctness and benchmark methodology are stable.
8. Adding domain logic to generic utility folders or hiding behavior in macros.
9. Preserving or copying implementation details merely for superficial similarity to
   a reference project.
10. Treating average throughput as the only performance metric; tail latency,
    allocations, determinism, and recovery matter as well.

## Engineering Principles

- Correctness precedes optimization.
- Deterministic command order is part of the public behavior.
- One module owns each invariant.
- Public headers stay small and stable.
- Side effects happen outside the matching decision.
- Every feature arrives with invalid-input tests and failure semantics.
- Measurements name their workload and environment.
- Documentation describes decisions and tradeoffs, not just class names.

## First Release Definition

The first release can be called a personal project milestone when it has:

- a clean CMake build from a fresh checkout;
- multi-instrument matching with documented order semantics;
- risk checks and explicit execution reports;
- journal, snapshot, and replay tests;
- sanitizer and static-analysis CI jobs;
- reproducible benchmark output with p50/p95/p99 latency; and
- README documentation that distinguishes simulation from production use.
