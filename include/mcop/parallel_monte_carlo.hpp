// parallel_monte_carlo.hpp — multi-threaded European Monte Carlo pricing.
//
// Splits the requested paths into contiguous blocks, one per worker. Each
// worker owns thread-local RNG state: pseudo-random workers get independent
// seeds, Sobol' workers skip-ahead to a disjoint block of the sequence. Partial
// accumulators are merged at the end, so the result for quasi-random sampling
// is identical regardless of thread count.
#pragma once

#include "mcop/engine.hpp"
#include "mcop/monte_carlo.hpp"
#include "mcop/option.hpp"

namespace mcop {

class ThreadPool;

// Price a European option using `pool` (or, if null, a pool sized to the
// hardware). `num_threads == 0` uses the pool's full width.
PricingResult monte_carlo_european_parallel(const OptionSpec& spec,
                                            const MarketData& market,
                                            const MonteCarloConfig& cfg,
                                            unsigned num_threads = 0,
                                            ThreadPool* pool = nullptr);

class ParallelMonteCarloEngine : public PricingEngine {
public:
    explicit ParallelMonteCarloEngine(MonteCarloConfig cfg = {}, unsigned threads = 0)
        : cfg_(cfg), threads_(threads) {}

    PricingResult price(const OptionSpec& spec,
                        const MarketData& market) const override {
        return monte_carlo_european_parallel(spec, market, cfg_, threads_);
    }
    std::string name() const override { return "Monte Carlo (parallel, GBM)"; }

private:
    MonteCarloConfig cfg_;
    unsigned threads_;
};

}  // namespace mcop
