// Phase 1 tests: option data models and math primitives.
#include "mcop/math_utils.hpp"
#include "mcop/option.hpp"
#include "mcop/version.hpp"
#include "test_framework.hpp"

#include <stdexcept>

using namespace mcop;

TEST(payoff_call_and_put) {
    OptionSpec call(100.0, 1.0, OptionType::Call, OptionStyle::European);
    OptionSpec put(100.0, 1.0, OptionType::Put, OptionStyle::American);
    CHECK_CLOSE(call.payoff(120.0), 20.0, 1e-12);
    CHECK_CLOSE(call.payoff(80.0), 0.0, 1e-12);
    CHECK_CLOSE(put.payoff(80.0), 20.0, 1e-12);
    CHECK_CLOSE(put.payoff(120.0), 0.0, 1e-12);
}

TEST(spec_validation) {
    bool threw = false;
    try {
        OptionSpec bad(-5.0, 1.0, OptionType::Call, OptionStyle::European);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(market_validation) {
    bool threw = false;
    try {
        MarketData bad(-1.0, 0.05, 0.2);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    CHECK(threw);
}

TEST(norm_cdf_known_values) {
    // Symmetry and reference points for the standard normal CDF.
    CHECK_CLOSE(math::norm_cdf(0.0), 0.5, 1e-12);
    CHECK_CLOSE(math::norm_cdf(1.0), 0.8413447460685429, 1e-12);
    CHECK_CLOSE(math::norm_cdf(-1.0), 0.1586552539314571, 1e-12);
    CHECK_CLOSE(math::norm_cdf(1.96), 0.9750021048517795, 1e-10);
    // Tail symmetry: N(x) + N(-x) == 1.
    CHECK_CLOSE(math::norm_cdf(2.3) + math::norm_cdf(-2.3), 1.0, 1e-12);
}

TEST(norm_pdf_known_values) {
    CHECK_CLOSE(math::norm_pdf(0.0), 0.3989422804014327, 1e-12);
    CHECK_CLOSE(math::norm_pdf(1.0), 0.24197072451914337, 1e-12);
}

TEST(version_string) {
    CHECK(std::string(version()).find("mcop") != std::string::npos);
}

int main() { return mcop::test::run_all(); }
