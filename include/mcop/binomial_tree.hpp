// binomial_tree.hpp — Cox-Ross-Rubinstein (CRR) binomial lattice engine.
//
// Prices both European and American options by backward induction. American
// early exercise is handled by taking max(intrinsic, continuation) at every
// node. Delta and gamma are extracted directly from the lattice's early
// nodes (no re-pricing required).
#pragma once

#include "mcop/engine.hpp"
#include "mcop/option.hpp"

namespace mcop {

PricingResult binomial_tree_price(const OptionSpec& spec, const MarketData& market,
                                  int steps = 1024);

class BinomialTreeEngine : public PricingEngine {
public:
    explicit BinomialTreeEngine(int steps = 1024) : steps_(steps) {}

    PricingResult price(const OptionSpec& spec,
                        const MarketData& market) const override {
        return binomial_tree_price(spec, market, steps_);
    }
    std::string name() const override { return "Cox-Ross-Rubinstein (binomial tree)"; }

    int steps() const { return steps_; }

private:
    int steps_;
};

}  // namespace mcop
