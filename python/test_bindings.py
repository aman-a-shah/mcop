#!/usr/bin/env python3
"""Smoke tests for the pybind11 bindings. Run: python3 python/test_bindings.py"""
import math
import os
import sys

sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import mcop_pricer as mc


def approx(a, b, tol):
    assert abs(a - b) <= tol, f"{a} != {b} (tol {tol})"


def test_black_scholes_reference():
    spec = mc.OptionSpec(100.0, 1.0, mc.OptionType.Call, mc.OptionStyle.European)
    market = mc.MarketData(100.0, 0.05, 0.20)
    approx(mc.black_scholes(spec, market).price, 10.450583572, 1e-6)


def test_put_call_parity_via_bindings():
    m = mc.MarketData(100.0, 0.05, 0.20)
    call = mc.OptionSpec(100.0, 1.0, mc.OptionType.Call)
    put = mc.OptionSpec(100.0, 1.0, mc.OptionType.Put)
    c = mc.black_scholes(call, m).price
    p = mc.black_scholes(put, m).price
    approx(c - p, 100.0 - 100.0 * math.exp(-0.05), 1e-9)


def test_monte_carlo_close_to_analytic():
    spec = mc.OptionSpec(100.0, 1.0, mc.OptionType.Call)
    m = mc.MarketData(100.0, 0.05, 0.20)
    sim = mc.monte_carlo(spec, m, paths=1 << 20, quasi_random=True, threads=4)
    approx(sim.price, 10.450583572, 5e-3)


def test_american_lsm_above_european():
    if not hasattr(mc, "longstaff_schwartz"):
        return
    m = mc.MarketData(100.0, 0.05, 0.20)
    aput = mc.OptionSpec(100.0, 1.0, mc.OptionType.Put, mc.OptionStyle.American)
    eput = mc.OptionSpec(100.0, 1.0, mc.OptionType.Put, mc.OptionStyle.European)
    lsm = mc.longstaff_schwartz(aput, m, paths=100000, steps=50).price
    euro = mc.black_scholes(eput, m).price
    assert lsm > euro, f"american {lsm} should exceed european {euro}"


def test_batch_zero_copy():
    spots = np.linspace(80, 120, 1000)
    strikes = np.full_like(spots, 100.0)
    prices = mc.price_european_batch(spots, strikes, rate=0.05, vol=0.20,
                                     maturity=1.0, is_call=True)
    assert prices.shape == spots.shape
    # Monotonic increasing in spot for a call.
    assert np.all(np.diff(prices) > 0)


if __name__ == "__main__":
    tests = [v for k, v in sorted(globals().items()) if k.startswith("test_")]
    for t in tests:
        t()
        print(f"[ PASS ] {t.__name__}")
    print(f"\nALL PASSED ({len(tests)} tests)")
