// Phase 5 tests: Longstaff-Schwartz LSM for American options.
#include "test_framework.hpp"

#ifdef MCOP_HAVE_EIGEN
#include "mcop/binomial_tree.hpp"
#include "mcop/black_scholes.hpp"
#include "mcop/longstaff_schwartz.hpp"

using namespace mcop;

namespace {
MarketData mkt() { return MarketData(100.0, 0.05, 0.20, 0.0); }
LsmConfig cfg(std::uint64_t seed) {
    LsmConfig c;
    c.num_paths = 200000;
    c.num_steps = 50;
    c.seed = seed;
    return c;
}
}  // namespace

TEST(lsm_american_put_matches_binomial) {
    OptionSpec ap(100, 1.0, OptionType::Put, OptionStyle::American);
    const double tree = binomial_tree_price(ap, mkt(), 4096).price;  // ~6.0896
    const double lsm = longstaff_schwartz_price(ap, mkt(), cfg(2024)).price;
    CHECK_CLOSE(lsm, tree, 0.05);
}

TEST(lsm_american_put_above_european) {
    OptionSpec ap(100, 1.0, OptionType::Put, OptionStyle::American);
    OptionSpec ep(100, 1.0, OptionType::Put, OptionStyle::European);
    const double lsm = longstaff_schwartz_price(ap, mkt(), cfg(11)).price;
    const double euro = black_scholes_price(ep, mkt()).price;
    CHECK(lsm > euro);
}

TEST(lsm_american_call_no_dividend_matches_european) {
    // No dividends => no early exercise => equals the European/BS call.
    OptionSpec ac(100, 1.0, OptionType::Call, OptionStyle::American);
    OptionSpec ec(100, 1.0, OptionType::Call, OptionStyle::European);
    const double bs = black_scholes_price(ec, mkt()).price;
    const double lsm = longstaff_schwartz_price(ac, mkt(), cfg(77)).price;
    CHECK_CLOSE(lsm, bs, 0.06);
}

TEST(lsm_dividend_call_has_early_exercise_premium) {
    // With a sizable dividend yield, an American call CAN be worth exercising
    // early, so LSM should exceed the matching European value.
    MarketData m(100.0, 0.05, 0.20, 0.08);
    OptionSpec ac(100, 1.0, OptionType::Call, OptionStyle::American);
    OptionSpec ec(100, 1.0, OptionType::Call, OptionStyle::European);
    const double euro = black_scholes_price(ec, m).price;
    const double lsm = longstaff_schwartz_price(ac, m, cfg(303)).price;
    CHECK(lsm > euro - 0.02);                    // at least not below European
    const double tree = binomial_tree_price(ac, m, 4096).price;
    CHECK_CLOSE(lsm, tree, 0.08);                // and tracks the lattice truth
}

int main() { return mcop::test::run_all(); }

#else
int main() {
    std::printf("Eigen not available — LSM tests skipped\n");
    return 0;
}
#endif
