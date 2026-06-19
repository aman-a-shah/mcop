# Project Summary — Monte Carlo Options Pricing Engine

A complete, self-contained briefing. Reading this gives you the problem, the
mathematics, the implementation, the API, and the measured results without
needing to open the source.

---

## 1. Problem & Objectives

Build a high-performance, multi-threaded **options pricing engine** in C++17
with a Python wrapper (pybind11). It prices **European** and **American**
options through three independent, cross-validating paradigms:

1. **Analytic** — closed-form Black-Scholes (the exact benchmark).
2. **Lattice** — Cox-Ross-Rubinstein binomial tree.
3. **Simulation** — Monte Carlo (GBM), plus Longstaff-Schwartz for American.

Having three engines that must agree is the core quality mechanism: the lattice
must converge to the analytic formula, and the Monte Carlo estimate must land
within its own standard-error band of both.

### Performance targets (from the PRD) vs. achieved

| Metric | Target | Achieved (10-core Apple Silicon) |
|--------|--------|----------------------------------|
| Throughput | ≥ 14M paths/sec | **~54M paths/sec** (10 threads) |
| Latency (1M-path valuation) | < 150 ms | **~15 ms** |
| Pricing error (MAE vs analytic) | < $0.04/contract | **~1e-5** |
| Variance-reduction efficiency | ~8× fewer paths | **6–55×** error reduction (Sobol vs antithetic pseudo-random) |

---

## 2. Mathematical Framework

### 2.1 Black-Scholes-Merton (European, closed form)

Under the risk-neutral measure with spot `S`, strike `K`, rate `r`, dividend
yield `q`, volatility `σ`, maturity `T`:

```
d1 = [ ln(S/K) + (r - q + σ²/2)·T ] / (σ·√T)
d2 = d1 - σ·√T

Call = S·e^(-qT)·N(d1) - K·e^(-rT)·N(d2)
Put  = K·e^(-rT)·N(-d2) - S·e^(-qT)·N(-d1)
```

`N(x)` is the standard normal CDF, computed **branchlessly** as
`0.5·erfc(-x/√2)` (avoids data-dependent branches in tight loops).

**Greeks** are closed-form (delta, gamma, vega, theta, rho), each handling the
dividend term. The degenerate limits `T→0` or `σ→0` return the discounted
intrinsic value. **Implied volatility** is solved with a hybrid Newton +
bisection iteration (Newton step, bisection fallback if it leaves the bracket).

Reference value used throughout: `S=K=100, r=5%, σ=20%, T=1, q=0` →
**Call = 10.450584**, **Put = 5.573526**, satisfying put-call parity
`C - P = S·e^(-qT) - K·e^(-rT)`.

### 2.2 Cox-Ross-Rubinstein binomial tree (European & American)

Time is discretized into `N` steps of `Δt = T/N`. Up/down moves and the
risk-neutral probability:

```
u = e^(σ√Δt),   d = 1/u,   p = (e^((r-q)Δt) - d) / (u - d)
```

Terminal asset prices `S_T = S0·u^j·d^(N-j)` seed the leaves with payoffs.
Backward induction discounts continuation values one step at a time:

```
V = e^(-rΔt)·(p·V_up + (1-p)·V_down)
```

For **American** options each node takes `max(intrinsic, continuation)`, which
captures early exercise. **Delta** and **gamma** come for free from the
lattice's step-1 and step-2 nodes (finite differences over real tree prices),
no re-pricing needed.

Cross-checks: converges to Black-Scholes as `N→∞`; an American call with no
dividend equals the European call (never optimal to exercise early); the
American put shows an early-exercise premium, reference **6.0896**.

### 2.3 Monte Carlo via Geometric Brownian Motion (European)

Risk-neutral GBM `dS = (r-q)S dt + σS dW`. A European payoff depends only on
the terminal price, sampled exactly (no time-stepping needed):

```
S_T = S0·exp( (r - q - σ²/2)·T + σ·√T·Z ),   Z ~ N(0,1)
price = e^(-rT) · mean( payoff(S_T) )
```

Reported with the **standard error** of the mean. **Pathwise Greeks** (exact
derivatives of the payoff, valid a.e. for the smooth GBM map):

```
delta (call) = e^(-rT)·(S_T/S0)·1{S_T>K}
vega  (call) = e^(-rT)·S_T·(√T·Z - σT)·1{S_T>K}
```

(put estimators carry the opposite sign and the `S_T<K` indicator).

### 2.4 Longstaff-Schwartz LSM (American)

Standard MC runs forward in time and is blind to early exercise. LSM (2001)
solves this with backward induction over simulated paths:

1. Simulate `M` forward GBM paths over `N` exercise dates (with antithetic
   pairing).
2. Initialize each path's cash flow to its **maturity** payoff.
3. Sweep **backward** `k = N-1 … 1`. At step `k`, for the **in-the-money**
   paths, regress the **discounted future cash flow** onto basis functions of
   the current asset price to estimate the **continuation value**.
4. Exercise early on a path when `intrinsic ≥ continuation` (record the cash
   flow and the exercise step).
5. Price = mean over paths of each cash flow discounted from its exercise step
   back to `t0`.

**Basis:** weighted **Laguerre polynomials** `L0..L_deg` (default degree 3 → 4
terms), evaluated at **moneyness `S/K`** to keep the design matrix
well-conditioned. Generated via the recurrence
`(k+1)L_{k+1} = (2k+1-x)L_k - k·L_{k-1}`.

**Regression:** ordinary least squares solved with **Eigen's column-pivoted
Householder QR** (`X.colPivHouseholderQr().solve(Y)`) — numerically stable
against the near-collinear polynomial design matrix (the reason orthogonal
Laguerre polynomials are preferred over raw `x, x², x³`).

European options skip the induction and reduce to plain discounted-payoff MC.
The whole engine is compiled only when Eigen is present (`MCOP_HAVE_EIGEN`).

Cross-checks: American put tracks the 4096-step lattice to within **0.05**
(LSM ≈ 6.078 vs 6.0896); no-dividend American call collapses to Black-Scholes;
a dividend-paying American call shows the expected early-exercise premium.

---

## 3. High-Performance Optimizations

### 3.1 Variance reduction (fewer paths for the same accuracy)

- **Sobol' low-discrepancy sequences.** Replace pseudo-random draws with a
  deterministic, maximally-uniform sequence, pushing integration error from
  `O(1/√N)` toward the quasi-random limit `~O(1/N)`. Implementation uses the
  classic **Numerical Recipes direction numbers** (primitive polynomials +
  Gray-code recurrence), supporting up to **6 dimensions**, 30-bit resolution.
  Uniform points are mapped to normals with **Acklam's inverse-normal CDF**
  plus one **Halley refinement** step (full double precision).
- **Antithetic variates.** For each draw `Z`, also evaluate `−Z`; the
  statistical sample is the pair average, which cancels odd-moment sampling
  bias and is reflected in a correctly-reduced standard error.

Measured: Sobol' cuts the European-call pricing error by **6–55×** versus
antithetic pseudo-random at the same path count.

### 3.2 Multi-threaded, lock-free path distribution

- A fixed-size **`ThreadPool`** (mutex/condvar task queue) distributes work.
- The path space is split into **contiguous, disjoint blocks**, one per worker.
  Workers share **no mutable state** during generation, so the hot loops are
  lock-free.
- **Thread-local RNG state**, the key correctness point:
  - *Pseudo-random* workers get independent, well-separated seeds
    (`seed + w·0x9E3779B97F4A7C15`).
  - *Sobol'* workers **skip-ahead** to their block's start index via an
    `O(bits·dim)` **Gray-code jump** (`set_index`): the Sobol state at index
    `n` is the XOR of the direction numbers selected by the set bits of
    `gray(n) = n ^ (n>>1)`. No overlap, no `O(N)` fast-forward.
- Partial accumulators merge **additively**, so a quasi-random price is
  **bit-identical regardless of thread count** (a strong test invariant).
- Build flags: `-O3 -march=native -funroll-loops` for SIMD (NEON on arm64,
  AVX on x86).

The classic interview pitfalls this addresses: a single mutex-guarded RNG
destroys parallelism (contention); an unseeded shared RNG destroys validity
(correlated paths). Per-thread independent state via seeds / Sobol skip-ahead
solves both.

---

## 4. Implementation & Code Layout

Language C++17; Eigen 5.0.1 (header-only, American LSM); pybind11 3.0.4 +
NumPy (Python). Build via CMake (≥3.18), globbed sources so files drop in
cleanly; CTest for tests. Eigen and pybind11 are auto-detected and optional.

```
include/mcop/
  option.hpp            OptionSpec, MarketData, PricingResult, enums, payoff
  engine.hpp            PricingEngine abstract interface
  math_utils.hpp        norm_pdf, norm_cdf (= 0.5·erfc(-x/√2)), constants
  inverse_normal.hpp    inv_norm_cdf (Acklam + Halley)
  black_scholes.hpp     analytic price + Greeks + implied vol
  binomial_tree.hpp     CRR lattice engine
  rng.hpp               NormalStream interface; PseudoNormalStream (mt19937_64)
  sobol.hpp             SobolGenerator (NR, ≤6 dims, set_index skip-ahead);
                        SobolNormalStream
  gbm_sample.hpp        shared GBM eval + Accumulator (serial & parallel reuse)
  monte_carlo.hpp       European MC config + engine
  mc_kernel.hpp         internal run_mc_block / finalize_mc declarations
  parallel_monte_carlo.hpp   multi-threaded European MC
  thread_pool.hpp       fixed-size worker pool (enqueue → future)
  longstaff_schwartz.hpp     American LSM (Eigen-gated)
  version.hpp
src/                    matching .cpp implementations
tests/                  test_framework.hpp + 7 C++ suites (CTest)
apps/price.cpp          CLI: runs every applicable engine and cross-checks
benchmarks/             convergence.cpp (pseudo vs Sobol), throughput.cpp
python/                 bindings.cpp, CMakeLists.txt, demo.py, test_bindings.py
Plan.md                 original PRD / mathematical spec
PHASES.md               incremental build history (8 phases)
```

### Core types

- `OptionSpec{ strike, maturity, type∈{Call,Put}, style∈{European,American} }`
  with `payoff(spot)` and constructor validation.
- `MarketData{ spot, rate, volatility, dividend }`.
- `PricingResult{ price, std_error, delta, gamma, vega, theta, rho }`.
- `PricingEngine` — abstract `price(spec, market)` + `name()`, implemented by
  `BlackScholesEngine`, `BinomialTreeEngine`, `MonteCarloEngine`,
  `ParallelMonteCarloEngine`, `LongstaffSchwartzEngine`.

### Design choices worth noting

- One **`NormalStream`** interface lets pseudo-random and Sobol' sources feed
  the same pricing loops unchanged.
- A single **`gbm_sample.hpp`** kernel holds the GBM math so the serial and
  parallel engines can never drift.
- Engine configs are plain structs (`MonteCarloConfig{ num_paths, num_steps,
  seed, antithetic, quasi_random }`, `LsmConfig{ num_paths, num_steps, seed,
  poly_degree, antithetic }`).

---

## 5. Public API

### C++ (free functions; engine classes wrap them)

```cpp
PricingResult black_scholes_price(spec, market);
double        black_scholes_implied_vol(spec, market, target_price, tol, max_iter);
PricingResult binomial_tree_price(spec, market, steps=1024);
PricingResult monte_carlo_european(spec, market, MonteCarloConfig{});
PricingResult monte_carlo_european_parallel(spec, market, cfg, num_threads=0, pool=nullptr);
PricingResult longstaff_schwartz_price(spec, market, LsmConfig{});   // Eigen
```

### Python (`mcop_pricer`)

```python
mc.black_scholes(spec, market) -> PricingResult
mc.binomial_tree(spec, market, steps=1024)
mc.monte_carlo(spec, market, paths=2**20, antithetic=True, quasi_random=True, threads=0)
mc.longstaff_schwartz(spec, market, paths=100000, steps=50, degree=3)   # if Eigen
mc.implied_vol(spec, market, target_price)
mc.price_european_batch(spot, strike, rate, vol, maturity, dividend=0, is_call=True)
```

`price_european_batch` reads NumPy `spot`/`strike` arrays **zero-copy**,
releases the GIL, and returns a freshly-allocated price array — values **1M
contracts in ~170 ms**. The bindings release the GIL around all heavy C++ work.

### CLI

```bash
./build/price --spot 100 --strike 100 --vol 0.2 --rate 0.05 --mat 1
./build/price --put --american --strike 100      # American: tree + LSM
./build/convergence                               # pseudo vs Sobol error table
./build/throughput                                # paths/sec scaling + latency
```

---

## 6. Results & Validation

All **8 test suites pass** (7 C++ via CTest + 1 Python smoke suite).

- **Black-Scholes:** matches textbook reference to 1e-6; all Greeks verified
  against finite differences; implied-vol round-trips; put-call parity to 1e-9.
- **Binomial:** converges to BS (price and Greeks) as steps grow; American
  put = 6.0896; no-dividend American call = European call.
- **Monte Carlo:** within ~4 standard errors of BS for calls and puts;
  antithetic shrinks the standard error; pathwise delta matches BS.
- **LSM:** American put within 0.05 of the lattice; above its European
  counterpart; dividend call shows early-exercise premium.
- **Sobol:** points in (0,1); 1-D mean → 0.5; 2-D quadrants ~25% each;
  inverse-normal round-trips; quasi beats pseudo at fixed N.
- **Parallel:** thread pool drains all tasks; Sobol skip-ahead bit-exact vs
  sequential; parallel price is thread-count-invariant and matches both the
  serial quasi result and Black-Scholes.

**Benchmarks (10-core):** ~54M paths/sec at 10 threads (13.7M at 1 thread);
1M-path valuation in ~15 ms; pricing error ~1e-5; Sobol 6–55× more accurate
than antithetic pseudo-random per path.

---

## 7. Build & Run

```bash
# Dependencies (macOS)
brew install eigen
pip install pybind11 numpy

# Configure, build, test
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DMCOP_BUILD_PYTHON=ON
cmake --build build -j
ctest --test-dir build --output-on-failure

# Python demo
python3 python/demo.py
```

Without Eigen the American/LSM engine is omitted but everything else builds;
without pybind11/`-DMCOP_BUILD_PYTHON=ON` the C++ library, CLI, tests, and
benchmarks still build.

---

## 8. Limitations & Possible Extensions

- Sobol' generator is capped at **6 dimensions** (NR table). High-dimensional
  path simulation (many exercise dates) with Sobol would want a larger
  direction-number set (e.g. Joe-Kuo) and a **Brownian-bridge** construction.
- `price_european_batch` is currently **Black-Scholes only**; a batched MC /
  GPU path would extend it.
- LSM reports a price and standard error but not full Greeks; American Greeks
  would need bumping or pathwise/likelihood-ratio extensions.
- Single-asset only — no multi-asset/basket payoffs or stochastic volatility.
- Sobol' integration is wired into the **European** MC (1-D); the LSM path
  generator uses pseudo-random draws.

---

## 9. One-paragraph TL;DR

A C++17/Python options pricer with three cross-validating engines: closed-form
Black-Scholes (price + Greeks + implied vol), a CRR binomial tree (European &
American with early exercise), and Monte Carlo over risk-neutral GBM. American
options use Longstaff-Schwartz least-squares MC with Eigen-backed QR regression
on a Laguerre-polynomial basis. Variance is reduced with Sobol' low-discrepancy
sequences (Numerical Recipes direction numbers, inverse-normal mapping) and
antithetic variates; throughput is scaled across cores with a lock-free thread
pool giving each worker thread-local RNG state (independent seeds, or Sobol'
Gray-code skip-ahead), so quasi-random prices are bit-identical regardless of
thread count. On a 10-core machine it hits ~54M paths/sec, values a 1M-path
option in ~15 ms, and prices to ~1e-5 against the analytic benchmark. Exposed to
Python via pybind11 with a zero-copy NumPy batch path (1M contracts in ~170 ms).
```
