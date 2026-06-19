// Convergence benchmark: pseudo-random vs Sobol' quasi-random Monte Carlo.
//
// Prints absolute pricing error against the Black-Scholes truth as the path
// count grows, illustrating the faster ~O(1/N) convergence of the
// low-discrepancy sequence versus ~O(1/sqrt(N)) for pseudo-random sampling.
#include "mcop/black_scholes.hpp"
#include "mcop/monte_carlo.hpp"

#include <cmath>
#include <cstdio>

using namespace mcop;

int main() {
    OptionSpec call(100, 1.0, OptionType::Call, OptionStyle::European);
    MarketData m(100.0, 0.05, 0.20, 0.0);
    const double truth = black_scholes_price(call, m).price;
    std::printf("Black-Scholes truth: %.6f\n\n", truth);
    std::printf("%10s %16s %16s %10s\n", "paths", "pseudo err", "sobol err",
                "speedup");

    for (std::size_t n = 1024; n <= 1u << 20; n *= 4) {
        MonteCarloConfig ps; ps.num_paths = n; ps.seed = 20240101; ps.antithetic = true;
        MonteCarloConfig qr; qr.num_paths = n; qr.quasi_random = true; qr.antithetic = true;
        const double e_ps = std::fabs(monte_carlo_european(call, m, ps).price - truth);
        const double e_qr = std::fabs(monte_carlo_european(call, m, qr).price - truth);
        std::printf("%10zu %16.2e %16.2e %9.1fx\n", n, e_ps, e_qr,
                    e_qr > 0 ? e_ps / e_qr : 0.0);
    }
    return 0;
}
