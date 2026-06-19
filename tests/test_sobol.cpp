// Phase 6 tests: Sobol' sequence properties and quasi-random convergence gain.
#include "mcop/black_scholes.hpp"
#include "mcop/monte_carlo.hpp"
#include "mcop/sobol.hpp"
#include "test_framework.hpp"

#include <vector>

using namespace mcop;

TEST(sobol_points_in_unit_interval) {
    SobolGenerator g(2);
    double pt[2];
    for (int i = 0; i < 1000; ++i) {
        g.next(pt);
        CHECK(pt[0] > 0.0 && pt[0] < 1.0);
        CHECK(pt[1] > 0.0 && pt[1] < 1.0);
    }
}

TEST(sobol_1d_mean_near_half) {
    // The average of a low-discrepancy 1-D sequence converges fast to 0.5.
    SobolGenerator g(1);
    double u, sum = 0.0;
    const int n = 8192;
    for (int i = 0; i < n; ++i) { g.next(&u); sum += u; }
    CHECK_CLOSE(sum / n, 0.5, 1e-3);
}

TEST(sobol_2d_covers_quadrants_evenly) {
    // Each of the four quadrants should hold ~1/4 of the points.
    SobolGenerator g(2);
    int quad[4] = {0, 0, 0, 0};
    const int n = 4096;
    double pt[2];
    for (int i = 0; i < n; ++i) {
        g.next(pt);
        quad[(pt[0] < 0.5 ? 0 : 1) + (pt[1] < 0.5 ? 0 : 2)]++;
    }
    for (int k = 0; k < 4; ++k) CHECK_CLOSE(quad[k] / double(n), 0.25, 0.02);
}

TEST(inverse_normal_round_trip) {
    for (double x : {-2.5, -1.0, -0.3, 0.0, 0.7, 1.5, 2.8}) {
        CHECK_CLOSE(math::inv_norm_cdf(math::norm_cdf(x)), x, 1e-9);
    }
}

TEST(quasi_random_beats_pseudo_for_european_call) {
    // At a fixed, modest path count, Sobol' should integrate the European call
    // payoff with materially smaller error than pseudo-random sampling.
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    MarketData m(100.0, 0.05, 0.20, 0.0);
    const double truth = black_scholes_price(call, m).price;

    MonteCarloConfig pseudo;
    pseudo.num_paths = 8192; pseudo.seed = 12345; pseudo.quasi_random = false;
    MonteCarloConfig quasi;
    quasi.num_paths = 8192; quasi.quasi_random = true;

    const double e_pseudo = std::fabs(monte_carlo_european(call, m, pseudo).price - truth);
    const double e_quasi = std::fabs(monte_carlo_european(call, m, quasi).price - truth);
    CHECK(e_quasi < e_pseudo);
}

int main() { return mcop::test::run_all(); }
