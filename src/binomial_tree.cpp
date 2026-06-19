#include "mcop/binomial_tree.hpp"

#include <cmath>
#include <stdexcept>
#include <vector>

namespace mcop {

PricingResult binomial_tree_price(const OptionSpec& spec, const MarketData& market,
                                  int steps) {
    if (steps < 2) throw std::invalid_argument("binomial tree requires >= 2 steps");

    const double S0 = market.spot;
    const double r = market.rate;
    const double q = market.dividend;
    const double sigma = market.volatility;
    const double T = spec.maturity;
    const bool american = (spec.style == OptionStyle::American);

    PricingResult res{};

    if (T <= 0.0) {
        res.price = spec.payoff(S0);
        return res;
    }

    const double dt = T / steps;
    const double u = std::exp(sigma * std::sqrt(dt));
    const double d = 1.0 / u;
    const double disc = std::exp(-r * dt);
    // Risk-neutral up-probability with continuous dividend yield.
    const double p = (std::exp((r - q) * dt) - d) / (u - d);

    // Terminal layer: asset price at node j is S0 * u^j * d^(steps-j).
    std::vector<double> value(steps + 1);
    for (int j = 0; j <= steps; ++j) {
        const double ST = S0 * std::pow(u, j) * std::pow(d, steps - j);
        value[j] = spec.payoff(ST);
    }

    // Captured value layers for analytic delta/gamma from the lattice root.
    double v1[2] = {0, 0};       // values at step 1
    double v2[3] = {0, 0, 0};    // values at step 2

    for (int i = steps - 1; i >= 0; --i) {
        for (int j = 0; j <= i; ++j) {
            double cont = disc * (p * value[j + 1] + (1.0 - p) * value[j]);
            if (american) {
                const double S = S0 * std::pow(u, j) * std::pow(d, i - j);
                cont = std::max(cont, spec.payoff(S));
            }
            value[j] = cont;
        }
        if (i == 2) { v2[0] = value[0]; v2[1] = value[1]; v2[2] = value[2]; }
        if (i == 1) { v1[0] = value[0]; v1[1] = value[1]; }
    }

    res.price = value[0];

    // Delta from the two step-1 nodes; gamma from the three step-2 nodes.
    const double S_up = S0 * u, S_dn = S0 * d;
    res.delta = (v1[1] - v1[0]) / (S_up - S_dn);

    const double Su2 = S0 * u * u, Smid = S0 * u * d, Sd2 = S0 * d * d;
    const double up_slope = (v2[2] - v2[1]) / (Su2 - Smid);
    const double dn_slope = (v2[1] - v2[0]) / (Smid - Sd2);
    res.gamma = (up_slope - dn_slope) / (0.5 * (Su2 - Sd2));

    return res;
}

}  // namespace mcop
