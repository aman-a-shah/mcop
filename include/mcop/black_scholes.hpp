// black_scholes.hpp — closed-form Black-Scholes-Merton analytic engine.
//
// This is the exact benchmark against which the lattice and Monte Carlo
// engines are validated. Supports a continuous dividend yield q.
#pragma once

#include "mcop/engine.hpp"
#include "mcop/option.hpp"

namespace mcop {

// Full analytic price + Greeks for a European option. Handles the degenerate
// T -> 0 and sigma -> 0 limits by returning the discounted intrinsic value.
PricingResult black_scholes_price(const OptionSpec& spec, const MarketData& market);

// Implied volatility that reproduces `target_price`, via a hybrid
// Newton/bisection solve. Returns NaN if no root exists in (1e-6, 5.0).
double black_scholes_implied_vol(const OptionSpec& spec, const MarketData& market,
                                 double target_price, double tol = 1e-8,
                                 int max_iter = 100);

class BlackScholesEngine : public PricingEngine {
public:
    PricingResult price(const OptionSpec& spec,
                        const MarketData& market) const override {
        return black_scholes_price(spec, market);
    }
    std::string name() const override { return "Black-Scholes (analytic)"; }
};

}  // namespace mcop
