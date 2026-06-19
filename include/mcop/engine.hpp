// engine.hpp — common interface implemented by every pricing engine.
//
// A uniform interface lets the analytic, lattice, and simulation engines be
// swapped and cross-validated against one another, and exposed identically
// through the Python layer.
#pragma once

#include "mcop/option.hpp"
#include <string>

namespace mcop {

class PricingEngine {
public:
    virtual ~PricingEngine() = default;

    // Price `spec` under `market`. Engines that cannot price a given style
    // (e.g. an analytic European formula asked for an American option) should
    // throw std::invalid_argument.
    virtual PricingResult price(const OptionSpec& spec,
                                const MarketData& market) const = 0;

    // Human-readable engine name (for logging / reporting).
    virtual std::string name() const = 0;
};

}  // namespace mcop
