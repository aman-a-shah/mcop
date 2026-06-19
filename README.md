# Monte Carlo Options Pricing Engine

A high-performance, multi-threaded options pricing engine in **C++17** with a
**Python** wrapper (pybind11). It prices European and American options through
three cross-validating paradigms:

```
                  ┌─────────────────────────────────────────┐
                  │          Options Pricing Engine          │
                  └────────────────────┬────────────────────┘
                                       │
         ┌─────────────────────────────┼─────────────────────────────┐
         ▼                             ▼                             ▼
┌──────────────────┐         ┌──────────────────┐         ┌──────────────────┐
│ Analytic Engine  │         │ Lattice Engine   │         │ Simulation Engine│
│ (Black-Scholes)  │         │ (Binomial Tree)  │         │ (Monte Carlo)    │
└──────────────────┘         └──────────────────┘         └──────────────────┘
```

## Status

Built incrementally — see [`PHASES.md`](PHASES.md). The full spec lives in
[`Plan.md`](Plan.md).

## Building

Requirements: a C++17 compiler, CMake ≥ 3.18. Eigen (for the Longstaff-Schwartz
engine) and pybind11 (for the Python module) are optional and auto-detected.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Layout

| Path | Contents |
|------|----------|
| `include/mcop/` | Public headers (data models, engines, math utilities) |
| `src/` | Engine implementations |
| `tests/` | CTest unit/validation suites |
| `apps/` | Command-line pricing tools |
| `benchmarks/` | Throughput & convergence benchmarks |
| `python/` | pybind11 module |

## Core abstractions

- `mcop::OptionSpec` — strike, maturity, type (Call/Put), style (European/American).
- `mcop::MarketData` — spot, risk-free rate, volatility, dividend yield.
- `mcop::PricingResult` — price, MC standard error, and Greeks.
