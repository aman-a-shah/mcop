#include "mcop/monte_carlo.hpp"

#include "mcop/gbm_sample.hpp"
#include "mcop/mc_kernel.hpp"
#include "mcop/rng.hpp"
#include "mcop/sobol.hpp"

#include <cmath>
#include <memory>
#include <stdexcept>

namespace mcop {

using detail::Accumulator;
using detail::eval_gbm;
using detail::GbmParams;

namespace detail {

// Run `n_samples` Monte Carlo samples starting at statistical-sample offset
// `start` into a fresh stream, accumulating into `acc`. Shared by the serial
// engine and each worker of the parallel engine. With antithetic on, each
// sample averages the z and -z payoffs and consumes one Sobol' point.
void run_mc_block(const GbmParams& p, const MonteCarloConfig& cfg,
                  std::size_t start, std::size_t n_samples, std::uint64_t seed,
                  Accumulator& acc) {
    std::unique_ptr<NormalStream> stream;
    if (cfg.quasi_random)
        stream = std::make_unique<SobolNormalStream>(1, static_cast<std::uint32_t>(start));
    else
        stream = std::make_unique<PseudoNormalStream>(seed);

    auto draw = [&] { double z; stream->fill(&z, 1); return z; };

    for (std::size_t i = 0; i < n_samples; ++i) {
        if (cfg.antithetic) {
            const double z = draw();
            const GbmSample a = eval_gbm(p, z), b = eval_gbm(p, -z);
            acc.add(0.5 * (a.payoff + b.payoff), 0.5 * (a.delta + b.delta),
                    0.5 * (a.vega + b.vega));
        } else {
            const GbmSample a = eval_gbm(p, draw());
            acc.add(a.payoff, a.delta, a.vega);
        }
    }
}

PricingResult finalize_mc(const Accumulator& acc) {
    PricingResult res{};
    if (acc.n == 0) return res;
    const double mean = acc.sum / acc.n;
    res.price = mean;
    res.delta = acc.delta / acc.n;
    res.vega = acc.vega / acc.n;
    if (acc.n > 1) {
        const double var = (acc.sum_sq - acc.n * mean * mean) / (acc.n - 1);
        res.std_error = std::sqrt(std::max(var, 0.0) / acc.n);
    }
    return res;
}

}  // namespace detail

PricingResult monte_carlo_european(const OptionSpec& spec, const MarketData& market,
                                   const MonteCarloConfig& cfg) {
    if (cfg.num_paths == 0) throw std::invalid_argument("num_paths must be > 0");

    if (spec.maturity <= 0.0) {
        PricingResult res{};
        res.price = spec.payoff(market.spot);
        return res;
    }

    const GbmParams p = GbmParams::from(spec, market);
    const std::size_t n_samples =
        cfg.antithetic ? (cfg.num_paths + 1) / 2 : cfg.num_paths;

    Accumulator acc;
    detail::run_mc_block(p, cfg, 0, n_samples, cfg.seed, acc);
    return detail::finalize_mc(acc);
}

}  // namespace mcop
