// Phase 2 tests: Black-Scholes analytic price, Greeks, parity, implied vol.
#include "mcop/black_scholes.hpp"
#include "test_framework.hpp"

using namespace mcop;

namespace {
OptionSpec call(double K = 100) {
    return OptionSpec(K, 1.0, OptionType::Call, OptionStyle::European);
}
OptionSpec put(double K = 100) {
    return OptionSpec(K, 1.0, OptionType::Put, OptionStyle::European);
}
MarketData mkt() { return MarketData(100.0, 0.05, 0.20, 0.0); }
}  // namespace

TEST(bs_reference_prices) {
    // Textbook values: S=100,K=100,r=5%,sigma=20%,T=1,q=0.
    CHECK_CLOSE(black_scholes_price(call(), mkt()).price, 10.450583572, 1e-6);
    CHECK_CLOSE(black_scholes_price(put(), mkt()).price, 5.573526022, 1e-6);
}

TEST(bs_put_call_parity) {
    // C - P = S e^{-qT} - K e^{-rT}.
    auto m = mkt();
    const double c = black_scholes_price(call(), m).price;
    const double p = black_scholes_price(put(), m).price;
    const double parity = m.spot * std::exp(-m.dividend * 1.0) -
                          100.0 * std::exp(-m.rate * 1.0);
    CHECK_CLOSE(c - p, parity, 1e-9);
}

TEST(bs_greeks_vs_finite_difference) {
    auto m = mkt();
    auto spec = call();
    const auto base = black_scholes_price(spec, m);

    // Delta / Gamma via bump in spot.
    const double h = 1e-4 * m.spot;
    MarketData up = m, dn = m;
    up.spot += h; dn.spot -= h;
    const double pu = black_scholes_price(spec, up).price;
    const double pd = black_scholes_price(spec, dn).price;
    CHECK_CLOSE(base.delta, (pu - pd) / (2 * h), 1e-5);
    CHECK_CLOSE(base.gamma, (pu - 2 * base.price + pd) / (h * h), 1e-3);

    // Vega via bump in volatility.
    const double hv = 1e-5;
    MarketData vu = m, vd = m;
    vu.volatility += hv; vd.volatility -= hv;
    const double dv = (black_scholes_price(spec, vu).price -
                       black_scholes_price(spec, vd).price) / (2 * hv);
    CHECK_CLOSE(base.vega, dv, 1e-3);

    // Rho via bump in rate.
    const double hr = 1e-6;
    MarketData ru = m, rd = m;
    ru.rate += hr; rd.rate -= hr;
    const double dr = (black_scholes_price(spec, ru).price -
                       black_scholes_price(spec, rd).price) / (2 * hr);
    CHECK_CLOSE(base.rho, dr, 1e-3);

    // Theta via bump in maturity (theta = -dV/dt = -dV/dT here).
    const double ht = 1e-6;
    OptionSpec su = spec, sd = spec;
    su.maturity += ht; sd.maturity -= ht;
    const double dt = -(black_scholes_price(su, m).price -
                        black_scholes_price(sd, m).price) / (2 * ht);
    CHECK_CLOSE(base.theta, dt, 1e-2);
}

TEST(bs_zero_time_is_intrinsic) {
    OptionSpec s(90.0, 0.0, OptionType::Call, OptionStyle::European);
    CHECK_CLOSE(black_scholes_price(s, mkt()).price, 10.0, 1e-12);
}

TEST(bs_implied_vol_round_trip) {
    auto m = mkt();
    auto spec = call();
    const double price = black_scholes_price(spec, m).price;
    const double iv = black_scholes_implied_vol(spec, m, price);
    CHECK_CLOSE(iv, 0.20, 1e-6);
}

int main() { return mcop::test::run_all(); }
