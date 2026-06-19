// option.hpp — core option contract and market data models.
//
// These value types are deliberately small and trivially copyable so they can
// be passed by value across thread boundaries and into the pybind11 layer
// without ownership concerns.
#pragma once

#include <algorithm>
#include <stdexcept>
#include <string>

namespace mcop {

// Right of the contract holder.
enum class OptionType { Call, Put };

// Exercise style. European => exercise only at expiry; American => exercise at
// any time up to and including expiry (handled via lattice / LSM engines).
enum class OptionStyle { European, American };

// Immutable description of an option contract.
struct OptionSpec {
    double strike{100.0};        // K — strike price
    double maturity{1.0};        // T — time to expiry, in years
    OptionType type{OptionType::Call};
    OptionStyle style{OptionStyle::European};

    OptionSpec() = default;
    OptionSpec(double K, double T, OptionType t, OptionStyle s)
        : strike(K), maturity(T), type(t), style(s) {
        if (K <= 0.0) throw std::invalid_argument("strike must be positive");
        if (T < 0.0) throw std::invalid_argument("maturity must be non-negative");
    }

    // Intrinsic payoff of the option for a given underlying price.
    double payoff(double spot) const {
        return (type == OptionType::Call) ? std::max(spot - strike, 0.0)
                                          : std::max(strike - spot, 0.0);
    }
};

// Market / model parameters under the risk-neutral measure.
struct MarketData {
    double spot{100.0};      // S0 — current underlying price
    double rate{0.05};       // r  — continuously-compounded risk-free rate
    double volatility{0.20}; // sigma — annualized volatility
    double dividend{0.0};    // q  — continuous dividend yield

    MarketData() = default;
    MarketData(double S, double r, double sigma, double q = 0.0)
        : spot(S), rate(r), volatility(sigma), dividend(q) {
        if (S <= 0.0) throw std::invalid_argument("spot must be positive");
        if (sigma < 0.0) throw std::invalid_argument("volatility must be non-negative");
    }
};

// Result bundle returned by the numerical engines. Greeks/std-error are
// optional and left as NaN by engines that do not compute them.
struct PricingResult {
    double price{0.0};
    double std_error{0.0};   // Monte Carlo standard error of the mean (0 if N/A)
    double delta{0.0};
    double gamma{0.0};
    double vega{0.0};
    double theta{0.0};
    double rho{0.0};
};

inline const char* to_string(OptionType t) {
    return t == OptionType::Call ? "Call" : "Put";
}
inline const char* to_string(OptionStyle s) {
    return s == OptionStyle::European ? "European" : "American";
}

}  // namespace mcop
