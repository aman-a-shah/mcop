// Phase 4 tests: Monte Carlo European pricing vs Black-Scholes.
#include "mcop/black_scholes.hpp"
#include "mcop/monte_carlo.hpp"
#include "test_framework.hpp"

using namespace mcop;

namespace {
MarketData mkt() { return MarketData(100.0, 0.05, 0.20, 0.0); }
}  // namespace

TEST(mc_european_call_within_error_band) {
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    const double bs = black_scholes_price(call, mkt()).price;
    MonteCarloConfig cfg;
    cfg.num_paths = 500000;
    cfg.seed = 42;
    const auto mc = monte_carlo_european(call, mkt(), cfg);
    CHECK(mc.std_error > 0.0);
    // Estimate must sit within ~4 standard errors of the analytic truth.
    CHECK(std::fabs(mc.price - bs) < 4.0 * mc.std_error);
}

TEST(mc_european_put_within_error_band) {
    OptionSpec put(100, 1.0, OptionType::Put, OptionStyle::European);
    const double bs = black_scholes_price(put, mkt()).price;
    MonteCarloConfig cfg;
    cfg.num_paths = 500000;
    cfg.seed = 7;
    const auto mc = monte_carlo_european(put, mkt(), cfg);
    CHECK(std::fabs(mc.price - bs) < 4.0 * mc.std_error);
}

TEST(mc_antithetic_reduces_standard_error) {
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    MonteCarloConfig plain;
    plain.num_paths = 200000; plain.seed = 99; plain.antithetic = false;
    MonteCarloConfig anti = plain; anti.antithetic = true;
    const double se_plain = monte_carlo_european(call, mkt(), plain).std_error;
    const double se_anti = monte_carlo_european(call, mkt(), anti).std_error;
    CHECK(se_anti < se_plain);
}

TEST(mc_pathwise_delta_matches_black_scholes) {
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    const double bs_delta = black_scholes_price(call, mkt()).delta;
    MonteCarloConfig cfg;
    cfg.num_paths = 500000; cfg.seed = 123; cfg.antithetic = true;
    const auto mc = monte_carlo_european(call, mkt(), cfg);
    CHECK_CLOSE(mc.delta, bs_delta, 5e-3);
}

int main() { return mcop::test::run_all(); }
