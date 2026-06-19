#!/usr/bin/env python3
"""Demo: drive the C++ pricing engine from Python.

Run from the repo root after building the module:
    cmake -S . -B build -DMCOP_BUILD_PYTHON=ON && cmake --build build -j
    python3 python/demo.py
"""
import os
import sys
import time

sys.path.insert(0, os.path.dirname(__file__))
import numpy as np
import mcop_pricer as mc


def main() -> None:
    print(f"mcop_pricer version {mc.__version__}\n")

    spec = mc.OptionSpec(strike=100.0, maturity=1.0, type=mc.OptionType.Call,
                         style=mc.OptionStyle.European)
    market = mc.MarketData(spot=100.0, rate=0.05, volatility=0.20)

    bs = mc.black_scholes(spec, market)
    print("European call  S=100 K=100 r=5% sigma=20% T=1")
    print(f"  black-scholes : {bs.price:.6f}  (delta={bs.delta:.4f}, "
          f"vega={bs.vega:.4f}, gamma={bs.gamma:.5f})")

    tree = mc.binomial_tree(spec, market, steps=2048)
    print(f"  binomial tree : {tree.price:.6f}")

    t0 = time.perf_counter()
    sim = mc.monte_carlo(spec, market, paths=1 << 22, quasi_random=True,
                         antithetic=True, threads=0)
    dt = (time.perf_counter() - t0) * 1e3
    print(f"  monte carlo   : {sim.price:.6f}  (std_err={sim.std_error:.2e}, "
          f"{1 << 22} paths in {dt:.1f} ms)")

    # American put — Longstaff-Schwartz, if Eigen was available at build time.
    if hasattr(mc, "longstaff_schwartz"):
        aput = mc.OptionSpec(100.0, 1.0, mc.OptionType.Put, mc.OptionStyle.American)
        lsm = mc.longstaff_schwartz(aput, market, paths=100000, steps=50)
        tree_p = mc.binomial_tree(aput, market, steps=2048)
        print(f"\nAmerican put   lsm={lsm.price:.4f}  binomial={tree_p.price:.4f}")

    # Zero-copy NumPy batch pricing across a grid of spots.
    spots = np.linspace(80, 120, 1_000_000)
    strikes = np.full_like(spots, 100.0)
    t0 = time.perf_counter()
    prices = mc.price_european_batch(spots, strikes, rate=0.05, vol=0.20,
                                     maturity=1.0, is_call=True)
    dt = (time.perf_counter() - t0) * 1e3
    print(f"\nbatch priced {prices.size:,} contracts in {dt:.1f} ms "
          f"(ATM={prices[len(prices)//2]:.4f})")


if __name__ == "__main__":
    main()
