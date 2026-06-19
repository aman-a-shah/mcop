// Phase 7 tests: parallel Monte Carlo correctness and determinism.
#include "mcop/black_scholes.hpp"
#include "mcop/monte_carlo.hpp"
#include "mcop/parallel_monte_carlo.hpp"
#include "mcop/sobol.hpp"
#include "mcop/thread_pool.hpp"
#include "test_framework.hpp"

#include <atomic>
#include <vector>

using namespace mcop;

namespace {
MarketData mkt() { return MarketData(100.0, 0.05, 0.20, 0.0); }
OptionSpec call() { return OptionSpec(100, 1.0, OptionType::Call, OptionStyle::European); }
}  // namespace

TEST(thread_pool_runs_all_tasks) {
    ThreadPool pool(4);
    std::atomic<int> counter{0};
    std::vector<std::future<int>> futs;
    for (int i = 0; i < 1000; ++i)
        futs.push_back(pool.enqueue([&counter] { return ++counter; }));
    for (auto& f : futs) f.get();
    CHECK(counter.load() == 1000);
}

TEST(sobol_skip_ahead_matches_sequential) {
    // After k sequential draws the generator is positioned at point k; so must
    // set_index(k). The next draw from each must be the bit-exact same point.
    SobolGenerator a(1), b(1);
    double x;
    for (int i = 0; i < 500; ++i) a.next(&x);  // consume points 0..499
    double xa; a.next(&xa);                    // point 500
    b.set_index(500);
    double xb; b.next(&xb);                    // point 500
    CHECK_CLOSE(xa, xb, 0.0);                  // bit-exact
}

TEST(parallel_quasi_is_thread_count_invariant) {
    // Sobol' partitioning is disjoint, so the price must not depend on threads.
    MonteCarloConfig cfg;
    cfg.num_paths = 1u << 18; cfg.quasi_random = true; cfg.antithetic = true;
    const double p1 = monte_carlo_european_parallel(call(), mkt(), cfg, 1).price;
    const double p8 = monte_carlo_european_parallel(call(), mkt(), cfg, 8).price;
    CHECK_CLOSE(p1, p8, 1e-9);
}

TEST(parallel_matches_serial_quasi) {
    MonteCarloConfig cfg;
    cfg.num_paths = 1u << 18; cfg.quasi_random = true;
    const double serial = monte_carlo_european(call(), mkt(), cfg).price;
    const double par = monte_carlo_european_parallel(call(), mkt(), cfg, 6).price;
    CHECK_CLOSE(serial, par, 1e-9);
}

TEST(parallel_prices_match_black_scholes) {
    const double bs = black_scholes_price(call(), mkt()).price;
    MonteCarloConfig cfg;
    cfg.num_paths = 1u << 20; cfg.quasi_random = true; cfg.antithetic = true;
    const double par = monte_carlo_european_parallel(call(), mkt(), cfg, 8).price;
    CHECK_CLOSE(par, bs, 5e-3);
}

int main() { return mcop::test::run_all(); }
