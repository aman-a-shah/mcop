// gbm_sample.hpp — shared single-step GBM terminal sampling for European
// payoffs, used by both the serial and parallel Monte Carlo engines so the
// pricing math lives in exactly one place.
#pragma once

#include "mcop/option.hpp"

#include <cmath>

namespace mcop {
namespace detail {

struct GbmParams {
    double S0, K, drift, vol, disc, sqrtT, sigma, T;
    bool is_call;

    static GbmParams from(const OptionSpec& spec, const MarketData& m) {
        GbmParams p{};
        p.S0 = m.spot;
        p.K = spec.strike;
        p.sigma = m.volatility;
        p.T = spec.maturity;
        p.drift = (m.rate - m.dividend - 0.5 * m.volatility * m.volatility) * p.T;
        p.vol = m.volatility * std::sqrt(p.T);
        p.disc = std::exp(-m.rate * p.T);
        p.sqrtT = std::sqrt(p.T);
        p.is_call = (spec.type == OptionType::Call);
        return p;
    }
};

struct GbmSample {
    double payoff;  // discounted payoff
    double delta;   // pathwise delta contribution
    double vega;    // pathwise vega contribution
};

// Evaluate one path given a standard-normal draw z.
inline GbmSample eval_gbm(const GbmParams& p, double z) {
    const double ST = p.S0 * std::exp(p.drift + p.vol * z);
    const double pay = p.is_call ? std::max(ST - p.K, 0.0) : std::max(p.K - ST, 0.0);
    const bool itm = p.is_call ? (ST > p.K) : (ST < p.K);
    const double sign = p.is_call ? 1.0 : -1.0;
    const double delta = itm ? p.disc * sign * (ST / p.S0) : 0.0;
    const double vega = itm ? p.disc * sign * ST * (p.sqrtT * z - p.sigma * p.T) : 0.0;
    return {p.disc * pay, delta, vega};
}

// Running accumulator over path samples; combine() merges thread-local partials.
struct Accumulator {
    double sum = 0.0, sum_sq = 0.0, delta = 0.0, vega = 0.0;
    std::size_t n = 0;

    void add(double payoff, double delta_c, double vega_c) {
        sum += payoff;
        sum_sq += payoff * payoff;
        delta += delta_c;
        vega += vega_c;
        ++n;
    }
    void combine(const Accumulator& o) {
        sum += o.sum; sum_sq += o.sum_sq; delta += o.delta; vega += o.vega; n += o.n;
    }
};

}  // namespace detail
}  // namespace mcop
