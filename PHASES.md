# Build Phases

This project is delivered in 8 incremental phases. Each phase is self-contained,
builds cleanly, passes its tests, and is committed before the next begins.

| # | Phase | Status |
|---|-------|--------|
| 1 | Scaffolding, build system & data models | ✅ done |
| 2 | Black-Scholes analytic engine | ✅ done |
| 3 | Binomial tree (CRR) lattice engine | ✅ done |
| 4 | Monte Carlo GBM path generator (European) | ✅ done |
| 5 | Longstaff-Schwartz LSM (American) with Eigen | ✅ done |
| 6 | Variance reduction (Sobol + antithetic) | ✅ done |
| 7 | Multi-threaded thread pool | ✅ done |
| 8 | Pybind11 wrapper, benchmarks & docs | ⏳ pending |

See `Plan.md` for the full product requirement document and mathematical spec.
