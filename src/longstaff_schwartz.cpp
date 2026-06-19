#include "mcop/longstaff_schwartz.hpp"

#ifdef MCOP_HAVE_EIGEN

#include "mcop/rng.hpp"

#include <Eigen/Dense>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace mcop {

namespace {

// Weighted Laguerre polynomials L_0..L_n evaluated at x, written into basis[].
// Using the recurrence (k+1)L_{k+1} = (2k+1-x)L_k - k L_{k-1}. The argument is
// option moneyness (S/K), which keeps the design matrix well-conditioned.
void laguerre_basis(double x, int degree, double* basis) {
    basis[0] = 1.0;
    if (degree >= 1) basis[1] = 1.0 - x;
    for (int k = 1; k < degree; ++k) {
        basis[k + 1] =
            ((2.0 * k + 1.0 - x) * basis[k] - k * basis[k - 1]) / (k + 1.0);
    }
}

}  // namespace

PricingResult longstaff_schwartz_price(const OptionSpec& spec,
                                       const MarketData& market,
                                       const LsmConfig& cfg) {
    if (cfg.num_paths == 0) throw std::invalid_argument("num_paths must be > 0");
    if (cfg.num_steps == 0) throw std::invalid_argument("num_steps must be > 0");

    const double S0 = market.spot;
    const double K = spec.strike;
    const double r = market.rate;
    const double q = market.dividend;
    const double sigma = market.volatility;
    const double T = spec.maturity;

    PricingResult res{};
    if (T <= 0.0) {
        res.price = spec.payoff(S0);
        return res;
    }

    const std::size_t N = cfg.num_steps;
    const double dt = T / static_cast<double>(N);
    const double drift = (r - q - 0.5 * sigma * sigma) * dt;
    const double vol = sigma * std::sqrt(dt);
    const double step_disc = std::exp(-r * dt);  // one-step discount factor

    // Build the path matrix. With antithetic pairing we generate M/2 base paths
    // and their mirror, so the effective sample stays cfg.num_paths.
    const bool anti = cfg.antithetic;
    const std::size_t M = anti ? ((cfg.num_paths + 1) / 2) * 2 : cfg.num_paths;

    // paths[i*N + (k-1)] holds S at step k (k = 1..N) for path i.
    std::vector<double> paths(M * N);
    PseudoNormalStream rng(cfg.seed);

    for (std::size_t i = 0; i < M; i += (anti ? 2 : 1)) {
        double s_a = S0, s_b = S0;
        for (std::size_t k = 0; k < N; ++k) {
            const double z = rng.next();
            s_a *= std::exp(drift + vol * z);
            paths[i * N + k] = s_a;
            if (anti) {
                s_b *= std::exp(drift - vol * z);
                paths[(i + 1) * N + k] = s_b;
            }
        }
    }

    // Cash flow bookkeeping: the step at which each path is (currently) assumed
    // to exercise, and the intrinsic value collected there.
    std::vector<std::size_t> exercise_step(M, N);
    std::vector<double> cashflow(M);
    for (std::size_t i = 0; i < M; ++i) {
        cashflow[i] = spec.payoff(paths[i * N + (N - 1)]);
    }

    const int deg = cfg.poly_degree;
    const int terms = deg + 1;
    std::vector<double> basis(terms);

    // Backward induction over the interior exercise dates k = N-1 .. 1.
    // European options have no early exercise, so the maturity cash flows
    // already hold and the induction is skipped entirely.
    for (std::size_t k = (spec.style == OptionStyle::American) ? N - 1 : 0;
         k >= 1; --k) {
        // Collect in-the-money paths at this step.
        std::vector<std::size_t> itm;
        itm.reserve(M);
        for (std::size_t i = 0; i < M; ++i) {
            if (spec.payoff(paths[i * N + (k - 1)]) > 0.0) itm.push_back(i);
        }

        // Need enough points to fit the basis; otherwise hold (no early exercise).
        if (static_cast<int>(itm.size()) > terms) {
            Eigen::MatrixXd X(itm.size(), terms);
            Eigen::VectorXd Y(itm.size());
            for (std::size_t row = 0; row < itm.size(); ++row) {
                const std::size_t i = itm[row];
                const double s = paths[i * N + (k - 1)];
                laguerre_basis(s / K, deg, basis.data());
                for (int c = 0; c < terms; ++c) X(row, c) = basis[c];
                // Discount this path's future cash flow back to step k.
                const double steps_ahead =
                    static_cast<double>(exercise_step[i] - k);
                Y(row) = cashflow[i] * std::pow(step_disc, steps_ahead);
            }

            // OLS via column-pivoted QR — numerically stable for the
            // (potentially collinear) polynomial design matrix.
            const Eigen::VectorXd beta = X.colPivHouseholderQr().solve(Y);

            for (std::size_t row = 0; row < itm.size(); ++row) {
                const std::size_t i = itm[row];
                const double s = paths[i * N + (k - 1)];
                const double intrinsic = spec.payoff(s);
                double continuation = 0.0;
                laguerre_basis(s / K, deg, basis.data());
                for (int c = 0; c < terms; ++c) continuation += beta(c) * basis[c];
                if (intrinsic >= continuation) {
                    exercise_step[i] = k;
                    cashflow[i] = intrinsic;
                }
            }
        }
        if (k == 1) break;  // guard: size_t loop, avoid underflow past 1
    }

    // Discount each path's realised cash flow from its exercise step back to t0
    // and average. Standard error is reported over the path estimates.
    double sum = 0.0, sum_sq = 0.0;
    for (std::size_t i = 0; i < M; ++i) {
        const double pv =
            cashflow[i] * std::pow(step_disc, static_cast<double>(exercise_step[i]));
        sum += pv;
        sum_sq += pv * pv;
    }
    const double mean = sum / M;
    res.price = mean;
    if (M > 1) {
        const double var = (sum_sq - M * mean * mean) / (M - 1);
        res.std_error = std::sqrt(std::max(var, 0.0) / M);
    }
    return res;
}

}  // namespace mcop

#endif  // MCOP_HAVE_EIGEN
