// mc_kernel.hpp — internal Monte Carlo kernel shared by the serial and
// parallel European engines.
#pragma once

#include "mcop/gbm_sample.hpp"
#include "mcop/monte_carlo.hpp"

#include <cstdint>

namespace mcop {
namespace detail {

// Accumulate `n_samples` samples (starting at statistical offset `start`) into
// `acc`, using a fresh stream seeded by `seed` (pseudo) or offset to `start`
// (Sobol').
void run_mc_block(const GbmParams& p, const MonteCarloConfig& cfg,
                  std::size_t start, std::size_t n_samples, std::uint64_t seed,
                  Accumulator& acc);

// Turn an accumulator into a PricingResult (price, std error, delta, vega).
PricingResult finalize_mc(const Accumulator& acc);

}  // namespace detail
}  // namespace mcop
