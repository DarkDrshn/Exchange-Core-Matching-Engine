# Day 1 Baseline

## Measurement Date

2026-09-07

## Scope

This is the migration baseline for Exchange Core. The new repository currently
contains planning documentation only, so these measurements come from the local
reference implementation at `HFT-inspo/matching-engine-cpp`.

The numbers are a starting point for comparison, not a universal performance claim.
They depend on hardware, compiler, build flags, operating-system scheduling, and the
benchmark workload.

## Reference Test Result

Command:

```text
ctest --test-dir build --output-on-failure
```

Result:

- 1/1 test targets passed.
- Exit code: 0.
- Test duration: approximately 0.02 seconds.

## Reference Benchmark Result

Command:

```text
./build/bench
```

Workload:

- 1,000,000 place/cancel pairs.
- Reference executable: `HFT-inspo/matching-engine-cpp/build/bench`.

Observed result on 2026-09-07:

```text
1,000,000 place+cancel pairs in 3,269,245 us
305,881 ops/sec
```

The benchmark exited successfully with code 0. It also printed an engine
initialization message; future benchmark output should be machine-readable and
should separate setup logging from measurements.

## New Repository Status

- Repository: `Exchange-Core-Matching-Engine`
- Branch: `project-foundation`
- Local build: not available yet; source files and CMake configuration are Day 2 work.
- Local tests: not available yet; the reference result above is not a substitute for
  tests compiled from this repository.

## Baseline Acceptance Criteria

Day 1 is complete when:

- the project purpose and non-goals are written down;
- this reference test and benchmark result is recorded with commands and date;
- the new repository has a dedicated foundation branch; and
- no performance claim is made without naming the workload and environment.

## Next Measurements

After the first implementation is migrated or reimplemented, repeat the same
place/cancel workload and record:

- compiler and version;
- CPU and operating system;
- CMake preset and build type;
- number of repetitions and warmup policy;
- elapsed time, operations/sec, and allocation behavior; and
- test count and complete CTest output.
