// longstaff_schwartz.hpp — Least-Squares Monte Carlo for American options.
//
// Implements the Longstaff-Schwartz (2001) algorithm: simulate forward GBM
// paths, then sweep backward in time estimating the continuation value of
// in-the-money paths by regressing discounted future cash flows onto a basis
// of Laguerre polynomials (Eigen-backed OLS). A path is exercised early
// whenever immediate intrinsic value exceeds the regressed continuation value.
//
// Requires Eigen (compiled only when MCOP_HAVE_EIGEN is defined).
#pragma once

#include "mcop/engine.hpp"
#include "mcop/option.hpp"

#include <cstdint>

namespace mcop {

struct LsmConfig {
    std::size_t num_paths{100000};  // simulated paths
    std::size_t num_steps{50};      // exercise dates / time steps
    std::uint64_t seed{2024};
    int poly_degree{3};             // Laguerre basis degree (=> degree+1 terms)
    bool antithetic{true};          // antithetic path pairs for variance reduction
};

// Price an American option via LSM. Falls back to plain discounted-payoff
// Monte Carlo if the option is European.
PricingResult longstaff_schwartz_price(const OptionSpec& spec,
                                       const MarketData& market,
                                       const LsmConfig& cfg = {});

class LongstaffSchwartzEngine : public PricingEngine {
public:
    explicit LongstaffSchwartzEngine(LsmConfig cfg = {}) : cfg_(cfg) {}

    PricingResult price(const OptionSpec& spec,
                        const MarketData& market) const override {
        return longstaff_schwartz_price(spec, market, cfg_);
    }
    std::string name() const override { return "Longstaff-Schwartz (LSM)"; }

    LsmConfig& config() { return cfg_; }

private:
    LsmConfig cfg_;
};

}  // namespace mcop
