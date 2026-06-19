// Throughput benchmark: path-generation rate and multi-core scaling.
//
// Measures simulated paths per second for the parallel European engine across
// thread counts, and the wall-clock latency for a standard valuation. Targets
// from the spec: >= 14M paths/sec across cores, and sub-150ms latency.
#include "mcop/black_scholes.hpp"
#include "mcop/monte_carlo.hpp"
#include "mcop/parallel_monte_carlo.hpp"

#include <chrono>
#include <cstdio>
#include <thread>

using namespace mcop;
using clk = std::chrono::steady_clock;

int main() {
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    MarketData m(100.0, 0.05, 0.20, 0.0);
    const double truth = black_scholes_price(call, m).price;

    const std::size_t paths = 1u << 24;  // ~16.7M paths
    MonteCarloConfig cfg;
    cfg.num_paths = paths;
    cfg.quasi_random = true;
    cfg.antithetic = true;

    const unsigned hw = std::thread::hardware_concurrency();
    std::printf("hardware threads: %u | paths/run: %zu\n", hw, paths);
    std::printf("%8s %12s %16s %12s %10s\n", "threads", "time(ms)", "paths/sec",
                "price", "abs err");

    double t1 = 0.0;
    for (unsigned t : {1u, 2u, 4u, 8u, hw}) {
        if (t == 0) continue;
        const auto start = clk::now();
        const auto res = monte_carlo_european_parallel(call, m, cfg, t);
        const double ms = std::chrono::duration<double, std::milli>(clk::now() - start).count();
        if (t == 1) t1 = ms;
        const double pps = paths / (ms / 1000.0);
        std::printf("%8u %12.1f %16.3e %12.5f %10.2e\n", t, ms, pps, res.price,
                    std::fabs(res.price - truth));
    }

    // Latency on a smaller, realistic valuation (1M paths).
    MonteCarloConfig fast = cfg;
    fast.num_paths = 1u << 20;
    const auto s = clk::now();
    monte_carlo_european_parallel(call, m, fast, hw);
    const double lat = std::chrono::duration<double, std::milli>(clk::now() - s).count();
    std::printf("\nlatency @ %u paths, %u threads: %.2f ms (target < 150)\n",
                1u << 20, hw, lat);
    if (t1 > 0) std::printf("speedup hint: 1-thread run took %.1f ms\n", t1);
    return 0;
}
