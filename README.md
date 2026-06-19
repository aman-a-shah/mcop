# Monte Carlo Options Pricing Engine

A high-performance, multi-threaded options pricing engine in **C++17** with a
zero-copy **Python** wrapper (pybind11). It prices European and American options
through three cross-validating paradigms:

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

Having three independent engines means every result is cross-checkable: the
binomial tree converges to Black-Scholes, and the Monte Carlo estimate lands
within its own standard-error band of both.

## Engines

| Engine | Options | Method |
|--------|---------|--------|
| **Black-Scholes** | European | Closed-form price + full Greeks (delta, gamma, vega, theta, rho), implied vol via Newton/bisection |
| **Binomial tree (CRR)** | European & American | Backward induction with early-exercise; lattice-derived delta/gamma |
| **Monte Carlo** | European | Risk-neutral GBM, pathwise Greeks, antithetic + Sobol' variance reduction, multi-threaded |
| **Longstaff-Schwartz** | American | Least-squares MC, Laguerre-polynomial regression (Eigen OLS) for continuation values |

## Performance

Measured on a 10-core Apple Silicon machine (`benchmarks/throughput`):

| Metric | Target | Achieved |
|--------|--------|----------|
| Throughput | ≥ 14M paths/sec | **~54M paths/sec** (10 threads) |
| Latency (1M-path valuation) | < 150 ms | **~15 ms** |
| Pricing error vs analytic | < $0.04 MAE | **~1e-5** |

Sobol' quasi-random sampling cuts the integration error by **6–55×** versus
antithetic pseudo-random at the same path count (`benchmarks/convergence`).

### Key optimizations

- **Sobol' low-discrepancy sequences** push convergence from O(1/√N) toward
  O(1/N) by filling the sample space uniformly instead of clustering.
- **Antithetic variates** evaluate `Z` and `−Z` together to cancel sampling bias.
- **Lock-free threading** gives each worker a disjoint block of paths with
  thread-local RNG state — pseudo-random workers use independent seeds, Sobol'
  workers skip-ahead via an O(bits·dim) Gray-code jump. Quasi-random prices are
  bit-identical regardless of thread count.
- **Branchless normal CDF** via `std::erfc`, and `-O3 -march=native` SIMD.

## Building

Requirements: a C++17 compiler and CMake ≥ 3.18. Eigen (American LSM) and
pybind11 + NumPy (Python module) are optional and auto-detected.

```bash
# macOS deps
brew install eigen
pip install pybind11 numpy

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMCOP_BUILD_PYTHON=ON
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Command-line usage

```bash
./build/price --spot 100 --strike 100 --vol 0.2 --rate 0.05 --mat 1
./build/price --put --american --strike 100        # American put, all engines
./build/convergence                                 # pseudo vs Sobol' error table
./build/throughput                                  # paths/sec scaling + latency
```

## Python usage

```python
import mcop_pricer as mc
import numpy as np

spec   = mc.OptionSpec(strike=100, maturity=1.0, type=mc.OptionType.Call)
market = mc.MarketData(spot=100, rate=0.05, volatility=0.20)

print(mc.black_scholes(spec, market).price)                 # 10.4506
print(mc.monte_carlo(spec, market, paths=2**22, threads=0)) # multi-threaded MC

# American option via Longstaff-Schwartz
aput = mc.OptionSpec(100, 1.0, mc.OptionType.Put, mc.OptionStyle.American)
print(mc.longstaff_schwartz(aput, market, paths=100_000, steps=50).price)

# Zero-copy vectorized batch pricing over NumPy arrays
spots   = np.linspace(80, 120, 1_000_000)
strikes = np.full_like(spots, 100.0)
prices  = mc.price_european_batch(spots, strikes, rate=0.05, vol=0.20, maturity=1.0)
```

See [`python/demo.py`](python/demo.py) for a full walkthrough.

## Layout

| Path | Contents |
|------|----------|
| `include/mcop/` | Public headers (data models, engines, RNG, math utilities) |
| `src/` | Engine implementations |
| `tests/` | CTest unit/validation suites (C++) |
| `apps/` | `price` command-line tool |
| `benchmarks/` | `convergence` & `throughput` benchmarks |
| `python/` | pybind11 module, demo, and binding tests |

## Core abstractions

- `mcop::OptionSpec` — strike, maturity, type (Call/Put), style (European/American).
- `mcop::MarketData` — spot, risk-free rate, volatility, dividend yield.
- `mcop::PricingResult` — price, MC standard error, and Greeks.
- `mcop::PricingEngine` — common interface implemented by every engine.

See [`Plan.md`](Plan.md) for the full PRD and mathematical spec, and
[`PHASES.md`](PHASES.md) for the incremental build history.
