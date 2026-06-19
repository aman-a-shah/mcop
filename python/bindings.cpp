// bindings.cpp — pybind11 module exposing the pricing engines to Python.
//
// In addition to scalar pricing, `price_european_batch` takes NumPy arrays by
// reference (zero-copy input) and writes prices into a freshly allocated output
// array, so a research script can value millions of contracts without any
// per-element Python overhead or serialization.
#include "mcop/binomial_tree.hpp"
#include "mcop/black_scholes.hpp"
#include "mcop/monte_carlo.hpp"
#include "mcop/option.hpp"
#include "mcop/parallel_monte_carlo.hpp"
#ifdef MCOP_HAVE_EIGEN
#include "mcop/longstaff_schwartz.hpp"
#endif

#include <pybind11/numpy.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <stdexcept>

namespace py = pybind11;
using namespace mcop;

namespace {

// Vectorized European Black-Scholes over NumPy arrays. Inputs are read in place
// (no copy); the result is one freshly allocated array of prices.
py::array_t<double> price_european_batch(py::array_t<double, py::array::c_style | py::array::forcecast> spot,
                                         py::array_t<double, py::array::c_style | py::array::forcecast> strike,
                                         double rate, double vol, double maturity,
                                         double dividend, bool is_call) {
    auto s = spot.unchecked<1>();
    auto k = strike.unchecked<1>();
    if (s.shape(0) != k.shape(0))
        throw std::invalid_argument("spot and strike must have equal length");

    const py::ssize_t n = s.shape(0);
    py::array_t<double> out(n);
    auto o = out.mutable_unchecked<1>();

    const OptionType type = is_call ? OptionType::Call : OptionType::Put;
    {
        py::gil_scoped_release release;  // pure C++ number crunching
        for (py::ssize_t i = 0; i < n; ++i) {
            OptionSpec spec(k(i), maturity, type, OptionStyle::European);
            MarketData m(s(i), rate, vol, dividend);
            o(i) = black_scholes_price(spec, m).price;
        }
    }
    return out;
}

}  // namespace

PYBIND11_MODULE(mcop_pricer, m) {
    m.doc() = "High-performance Monte Carlo options pricing engine (C++ core)";

    py::enum_<OptionType>(m, "OptionType")
        .value("Call", OptionType::Call)
        .value("Put", OptionType::Put);

    py::enum_<OptionStyle>(m, "OptionStyle")
        .value("European", OptionStyle::European)
        .value("American", OptionStyle::American);

    py::class_<OptionSpec>(m, "OptionSpec")
        .def(py::init<double, double, OptionType, OptionStyle>(), py::arg("strike"),
             py::arg("maturity"), py::arg("type") = OptionType::Call,
             py::arg("style") = OptionStyle::European)
        .def_readwrite("strike", &OptionSpec::strike)
        .def_readwrite("maturity", &OptionSpec::maturity)
        .def_readwrite("type", &OptionSpec::type)
        .def_readwrite("style", &OptionSpec::style)
        .def("payoff", &OptionSpec::payoff, py::arg("spot"));

    py::class_<MarketData>(m, "MarketData")
        .def(py::init<double, double, double, double>(), py::arg("spot"),
             py::arg("rate"), py::arg("volatility"), py::arg("dividend") = 0.0)
        .def_readwrite("spot", &MarketData::spot)
        .def_readwrite("rate", &MarketData::rate)
        .def_readwrite("volatility", &MarketData::volatility)
        .def_readwrite("dividend", &MarketData::dividend);

    py::class_<PricingResult>(m, "PricingResult")
        .def_readonly("price", &PricingResult::price)
        .def_readonly("std_error", &PricingResult::std_error)
        .def_readonly("delta", &PricingResult::delta)
        .def_readonly("gamma", &PricingResult::gamma)
        .def_readonly("vega", &PricingResult::vega)
        .def_readonly("theta", &PricingResult::theta)
        .def_readonly("rho", &PricingResult::rho)
        .def("__repr__", [](const PricingResult& r) {
            return "<PricingResult price=" + std::to_string(r.price) +
                   " std_error=" + std::to_string(r.std_error) + ">";
        });

    // --- scalar engines ---
    m.def("black_scholes", &black_scholes_price, py::arg("spec"), py::arg("market"),
          "Closed-form Black-Scholes price + Greeks (European).");
    m.def("implied_vol", &black_scholes_implied_vol, py::arg("spec"),
          py::arg("market"), py::arg("target_price"), py::arg("tol") = 1e-8,
          py::arg("max_iter") = 100);
    m.def("binomial_tree", &binomial_tree_price, py::arg("spec"), py::arg("market"),
          py::arg("steps") = 1024, "Cox-Ross-Rubinstein lattice (European/American).");

    m.def(
        "monte_carlo",
        [](const OptionSpec& spec, const MarketData& market, std::size_t paths,
           bool antithetic, bool quasi_random, unsigned threads) {
            MonteCarloConfig cfg;
            cfg.num_paths = paths;
            cfg.antithetic = antithetic;
            cfg.quasi_random = quasi_random;
            py::gil_scoped_release release;
            return threads <= 1
                       ? monte_carlo_european(spec, market, cfg)
                       : monte_carlo_european_parallel(spec, market, cfg, threads);
        },
        py::arg("spec"), py::arg("market"), py::arg("paths") = 1u << 20,
        py::arg("antithetic") = true, py::arg("quasi_random") = true,
        py::arg("threads") = 0, "Monte Carlo price for a European option.");

#ifdef MCOP_HAVE_EIGEN
    m.def(
        "longstaff_schwartz",
        [](const OptionSpec& spec, const MarketData& market, std::size_t paths,
           std::size_t steps, int degree) {
            LsmConfig cfg;
            cfg.num_paths = paths;
            cfg.num_steps = steps;
            cfg.poly_degree = degree;
            py::gil_scoped_release release;
            return longstaff_schwartz_price(spec, market, cfg);
        },
        py::arg("spec"), py::arg("market"), py::arg("paths") = 100000,
        py::arg("steps") = 50, py::arg("degree") = 3,
        "Longstaff-Schwartz LSM price for an American option.");
#endif

    m.def("price_european_batch", &price_european_batch, py::arg("spot"),
          py::arg("strike"), py::arg("rate"), py::arg("vol"), py::arg("maturity"),
          py::arg("dividend") = 0.0, py::arg("is_call") = true,
          "Vectorized European Black-Scholes over NumPy spot/strike arrays.");

    m.attr("__version__") = "0.1.0";
}
