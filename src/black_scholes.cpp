#include "mcop/black_scholes.hpp"

#include "mcop/math_utils.hpp"

#include <cmath>
#include <limits>

namespace mcop {

using math::norm_cdf;
using math::norm_pdf;

PricingResult black_scholes_price(const OptionSpec& spec, const MarketData& market) {
    const double S = market.spot;
    const double K = spec.strike;
    const double r = market.rate;
    const double q = market.dividend;
    const double sigma = market.volatility;
    const double T = spec.maturity;
    const bool is_call = (spec.type == OptionType::Call);

    PricingResult res{};

    // Degenerate limit: no time value left, or zero volatility. The option is
    // worth the discounted forward intrinsic value. Greeks collapse to the
    // sub-/super-gradient; we report the well-defined ones.
    if (T <= 0.0 || sigma <= 0.0) {
        const double fwd = S * std::exp((r - q) * T);
        const double disc = std::exp(-r * T);
        const double intrinsic = is_call ? std::max(fwd - K, 0.0)
                                         : std::max(K - fwd, 0.0);
        res.price = disc * intrinsic;
        const bool itm = is_call ? (fwd > K) : (fwd < K);
        res.delta = itm ? (is_call ? std::exp(-q * T) : -std::exp(-q * T)) : 0.0;
        res.gamma = 0.0;
        res.vega = 0.0;
        res.theta = 0.0;
        res.rho = 0.0;
        return res;
    }

    const double sqrtT = std::sqrt(T);
    const double vol_sqrtT = sigma * sqrtT;
    const double d1 = (std::log(S / K) + (r - q + 0.5 * sigma * sigma) * T) / vol_sqrtT;
    const double d2 = d1 - vol_sqrtT;

    const double disc_r = std::exp(-r * T);  // risk-free discount
    const double disc_q = std::exp(-q * T);  // dividend discount

    const double Nd1 = norm_cdf(d1);
    const double Nd2 = norm_cdf(d2);
    const double pdf_d1 = norm_pdf(d1);

    if (is_call) {
        res.price = S * disc_q * Nd1 - K * disc_r * Nd2;
        res.delta = disc_q * Nd1;
        res.theta = -(S * disc_q * pdf_d1 * sigma) / (2.0 * sqrtT)
                    - r * K * disc_r * Nd2 + q * S * disc_q * Nd1;
        res.rho = K * T * disc_r * Nd2;
    } else {
        const double Nmd1 = norm_cdf(-d1);
        const double Nmd2 = norm_cdf(-d2);
        res.price = K * disc_r * Nmd2 - S * disc_q * Nmd1;
        res.delta = -disc_q * Nmd1;
        res.theta = -(S * disc_q * pdf_d1 * sigma) / (2.0 * sqrtT)
                    + r * K * disc_r * Nmd2 - q * S * disc_q * Nmd1;
        res.rho = -K * T * disc_r * Nmd2;
    }

    // Gamma and vega are identical for calls and puts.
    res.gamma = disc_q * pdf_d1 / (S * vol_sqrtT);
    res.vega = S * disc_q * pdf_d1 * sqrtT;  // per 1.00 (100%) change in vol

    return res;
}

double black_scholes_implied_vol(const OptionSpec& spec, const MarketData& market,
                                 double target_price, double tol, int max_iter) {
    // Bracket on (lo, hi); Newton step with bisection fallback for robustness.
    double lo = 1e-6, hi = 5.0;
    MarketData m = market;

    auto price_at = [&](double vol) {
        m.volatility = vol;
        return black_scholes_price(spec, m).price;
    };

    const double p_lo = price_at(lo) - target_price;
    const double p_hi = price_at(hi) - target_price;
    if (p_lo * p_hi > 0.0) return std::numeric_limits<double>::quiet_NaN();

    double vol = 0.2;  // sensible starting guess
    for (int i = 0; i < max_iter; ++i) {
        m.volatility = vol;
        const PricingResult r = black_scholes_price(spec, m);
        const double diff = r.price - target_price;
        if (std::fabs(diff) < tol) return vol;

        // Maintain the bracket.
        if (diff > 0.0) hi = vol; else lo = vol;

        double next = vol - diff / r.vega;  // Newton
        if (!(next > lo && next < hi)) next = 0.5 * (lo + hi);  // bisection fallback
        vol = next;
    }
    return vol;
}

}  // namespace mcop
