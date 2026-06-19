#include "mcop/monte_carlo.hpp"

#include "mcop/rng.hpp"

#include <cmath>
#include <stdexcept>

namespace mcop {

PricingResult monte_carlo_european(const OptionSpec& spec, const MarketData& market,
                                   const MonteCarloConfig& cfg) {
    if (cfg.num_paths == 0) throw std::invalid_argument("num_paths must be > 0");

    const double S0 = market.spot;
    const double K = spec.strike;
    const double r = market.rate;
    const double q = market.dividend;
    const double sigma = market.volatility;
    const double T = spec.maturity;
    const bool is_call = (spec.type == OptionType::Call);

    PricingResult res{};
    if (T <= 0.0) {
        res.price = spec.payoff(S0);
        return res;
    }

    const double drift = (r - q - 0.5 * sigma * sigma) * T;
    const double vol = sigma * std::sqrt(T);
    const double disc = std::exp(-r * T);
    const double sqrtT = std::sqrt(T);

    // Evaluate one sample given a normal draw Z; returns the discounted payoff
    // plus pathwise delta/vega contributions for that draw.
    struct Sample { double payoff, delta, vega; };
    auto eval = [&](double z) -> Sample {
        const double ST = S0 * std::exp(drift + vol * z);
        const double pay = spec.payoff(ST);
        const bool itm = is_call ? (ST > K) : (ST < K);
        // Pathwise estimators (exact for the smooth GBM payoff a.e.).
        const double sign = is_call ? 1.0 : -1.0;
        const double delta = itm ? disc * sign * (ST / S0) : 0.0;
        const double vega = itm ? disc * sign * ST * (sqrtT * z - sigma * T) : 0.0;
        return {disc * pay, delta, vega};
    };

    PseudoNormalStream rng(cfg.seed);

    double sum = 0.0, sum_sq = 0.0;
    double delta_sum = 0.0, vega_sum = 0.0;
    std::size_t n_stat = 0;

    if (cfg.antithetic) {
        // Each statistical sample is the average of the Z and -Z payoffs, so
        // the reported standard error reflects the variance reduction.
        const std::size_t pairs = (cfg.num_paths + 1) / 2;
        for (std::size_t i = 0; i < pairs; ++i) {
            const double z = rng.next();
            const Sample a = eval(z), b = eval(-z);
            const double s = 0.5 * (a.payoff + b.payoff);
            sum += s;
            sum_sq += s * s;
            delta_sum += 0.5 * (a.delta + b.delta);
            vega_sum += 0.5 * (a.vega + b.vega);
            ++n_stat;
        }
    } else {
        for (std::size_t i = 0; i < cfg.num_paths; ++i) {
            const Sample a = eval(rng.next());
            sum += a.payoff;
            sum_sq += a.payoff * a.payoff;
            delta_sum += a.delta;
            vega_sum += a.vega;
            ++n_stat;
        }
    }

    const double mean = sum / n_stat;
    res.price = mean;
    res.delta = delta_sum / n_stat;
    res.vega = vega_sum / n_stat;

    if (n_stat > 1) {
        const double var = (sum_sq - n_stat * mean * mean) / (n_stat - 1);
        res.std_error = std::sqrt(std::max(var, 0.0) / n_stat);
    }
    return res;
}

}  // namespace mcop
