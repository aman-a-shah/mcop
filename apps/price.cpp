// price — command-line option pricer that runs every engine and cross-checks.
//
// Usage:
//   price [--spot S] [--strike K] [--rate r] [--vol s] [--mat T] [--div q]
//         [--put] [--american] [--paths N] [--steps M]
#include "mcop/binomial_tree.hpp"
#include "mcop/black_scholes.hpp"
#include "mcop/monte_carlo.hpp"
#include "mcop/parallel_monte_carlo.hpp"
#ifdef MCOP_HAVE_EIGEN
#include "mcop/longstaff_schwartz.hpp"
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace mcop;

int main(int argc, char** argv) {
    double spot = 100, strike = 100, rate = 0.05, vol = 0.20, mat = 1.0, div = 0.0;
    bool is_put = false, american = false;
    std::size_t paths = 1u << 20;
    int steps = 2048;

    for (int i = 1; i < argc; ++i) {
        auto next = [&](double& dst) { if (i + 1 < argc) dst = std::atof(argv[++i]); };
        std::string a = argv[i];
        if (a == "--spot") next(spot);
        else if (a == "--strike") next(strike);
        else if (a == "--rate") next(rate);
        else if (a == "--vol") next(vol);
        else if (a == "--mat") next(mat);
        else if (a == "--div") next(div);
        else if (a == "--put") is_put = true;
        else if (a == "--american") american = true;
        else if (a == "--paths" && i + 1 < argc) paths = std::strtoull(argv[++i], nullptr, 10);
        else if (a == "--steps" && i + 1 < argc) steps = std::atoi(argv[++i]);
        else { std::printf("unknown arg: %s\n", a.c_str()); return 1; }
    }

    const OptionType type = is_put ? OptionType::Put : OptionType::Call;
    const OptionStyle style = american ? OptionStyle::American : OptionStyle::European;
    OptionSpec spec(strike, mat, type, style);
    MarketData market(spot, rate, vol, div);

    std::printf("%s %s  S=%.2f K=%.2f r=%.3f sigma=%.3f T=%.3f q=%.3f\n\n",
                to_string(style), to_string(type), spot, strike, rate, vol, mat, div);

    if (style == OptionStyle::European) {
        const auto bs = black_scholes_price(spec, market);
        std::printf("  black-scholes : %.6f   (delta=%.4f gamma=%.5f vega=%.4f "
                    "theta=%.4f rho=%.4f)\n",
                    bs.price, bs.delta, bs.gamma, bs.vega, bs.theta, bs.rho);
    }

    const auto tree = binomial_tree_price(spec, market, steps);
    std::printf("  binomial tree : %.6f   (%d steps)\n", tree.price, steps);

    if (style == OptionStyle::European) {
        MonteCarloConfig cfg;
        cfg.num_paths = paths; cfg.quasi_random = true; cfg.antithetic = true;
        const auto mc = monte_carlo_european_parallel(spec, market, cfg);
        std::printf("  monte carlo   : %.6f   (std_err=%.2e, %zu paths)\n",
                    mc.price, mc.std_error, paths);
    }
#ifdef MCOP_HAVE_EIGEN
    if (style == OptionStyle::American) {
        LsmConfig cfg; cfg.num_paths = 200000; cfg.num_steps = 50;
        const auto lsm = longstaff_schwartz_price(spec, market, cfg);
        std::printf("  longstaff-schwartz: %.6f   (lsm, 200k paths x 50 steps)\n",
                    lsm.price);
    }
#endif
    return 0;
}
