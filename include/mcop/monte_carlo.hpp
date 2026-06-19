// monte_carlo.hpp — Monte Carlo simulation engine (European options).
//
// Prices via risk-neutral Geometric Brownian Motion. For a European payoff the
// terminal price is sampled directly:
//   S_T = S0 * exp((r - q - 0.5*sigma^2)*T + sigma*sqrt(T)*Z),  Z ~ N(0,1)
// The price is the discounted sample mean of the payoff, reported with its
// Monte Carlo standard error.
#pragma once

#include "mcop/engine.hpp"
#include "mcop/option.hpp"

#include <cstdint>

namespace mcop {

struct MonteCarloConfig {
    std::size_t num_paths{200000};   // number of simulated paths
    std::size_t num_steps{1};        // time-discretization steps (1 = exact terminal)
    std::uint64_t seed{12345};       // base RNG seed (pseudo-random only)
    bool antithetic{false};          // antithetic variates
    bool quasi_random{false};        // use Sobol' low-discrepancy sequence
};

// Price a European option by direct terminal sampling. Populates price,
// std_error, and pathwise delta/vega estimators.
PricingResult monte_carlo_european(const OptionSpec& spec, const MarketData& market,
                                   const MonteCarloConfig& cfg = {});

class MonteCarloEngine : public PricingEngine {
public:
    explicit MonteCarloEngine(MonteCarloConfig cfg = {}) : cfg_(cfg) {}

    PricingResult price(const OptionSpec& spec,
                        const MarketData& market) const override {
        return monte_carlo_european(spec, market, cfg_);
    }
    std::string name() const override { return "Monte Carlo (GBM, pseudo-random)"; }

    MonteCarloConfig& config() { return cfg_; }

private:
    MonteCarloConfig cfg_;
};

}  // namespace mcop
