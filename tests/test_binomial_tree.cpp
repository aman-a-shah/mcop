// Phase 3 tests: CRR binomial tree — convergence to BS, American premium.
#include "mcop/binomial_tree.hpp"
#include "mcop/black_scholes.hpp"
#include "test_framework.hpp"

using namespace mcop;

namespace {
MarketData mkt() { return MarketData(100.0, 0.05, 0.20, 0.0); }
}  // namespace

TEST(crr_european_converges_to_black_scholes) {
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    OptionSpec put(100, 1.0, OptionType::Put, OptionStyle::European);
    const double bs_call = black_scholes_price(call, mkt()).price;
    const double bs_put = black_scholes_price(put, mkt()).price;
    // 4096-step tree should match analytic to a couple of cents.
    CHECK_CLOSE(binomial_tree_price(call, mkt(), 4096).price, bs_call, 5e-3);
    CHECK_CLOSE(binomial_tree_price(put, mkt(), 4096).price, bs_put, 5e-3);
}

TEST(crr_convergence_improves_with_steps) {
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    const double bs = black_scholes_price(call, mkt()).price;
    const double e_coarse = std::fabs(binomial_tree_price(call, mkt(), 64).price - bs);
    const double e_fine = std::fabs(binomial_tree_price(call, mkt(), 2048).price - bs);
    CHECK(e_fine < e_coarse);
}

TEST(crr_american_call_no_dividend_equals_european) {
    // With no dividends an American call is never exercised early.
    OptionSpec ac(100, 1.0, OptionType::Call, OptionStyle::American);
    OptionSpec ec(100, 1.0, OptionType::Call, OptionStyle::European);
    CHECK_CLOSE(binomial_tree_price(ac, mkt(), 2048).price,
                binomial_tree_price(ec, mkt(), 2048).price, 1e-9);
}

TEST(crr_american_put_has_early_exercise_premium) {
    OptionSpec ap(100, 1.0, OptionType::Put, OptionStyle::American);
    OptionSpec ep(100, 1.0, OptionType::Put, OptionStyle::European);
    const double am = binomial_tree_price(ap, mkt(), 4096).price;
    const double eu = binomial_tree_price(ep, mkt(), 4096).price;
    CHECK(am > eu);                 // early exercise is worth something
    CHECK_CLOSE(am, 6.0896, 5e-3);  // standard reference value
}

TEST(crr_greeks_match_black_scholes) {
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    const auto bs = black_scholes_price(call, mkt());
    const auto tree = binomial_tree_price(call, mkt(), 4096);
    CHECK_CLOSE(tree.delta, bs.delta, 5e-3);
    CHECK_CLOSE(tree.gamma, bs.gamma, 5e-3);
}

int main() { return mcop::test::run_all(); }
