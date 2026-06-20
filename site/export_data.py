#!/usr/bin/env python3
"""Generate site/data.json from real runs of the C++ pricing engine.

Every figure and statistic on the showcase page is produced here — there is no
hand-entered data. Run from the repo root:

    cmake -S . -B build -DMCOP_BUILD_PYTHON=ON && cmake --build build -j
    python3 site/export_data.py
"""
import json
import math
import os
import sys
import time

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "python"))

import numpy as np
import mcop_pricer as mc

# ---------------------------------------------------------------- reference
S0, K, R, VOL, T, Q = 100.0, 100.0, 0.05, 0.20, 1.0, 0.0
call = mc.OptionSpec(K, T, mc.OptionType.Call, mc.OptionStyle.European)
put = mc.OptionSpec(K, T, mc.OptionType.Put, mc.OptionStyle.European)
market = mc.MarketData(S0, R, VOL, Q)

bs_call = mc.black_scholes(call, market)
bs_put = mc.black_scholes(put, market)
BS_TRUTH = bs_call.price


def bs_price(spot, strike, is_call, vol=VOL, t=T):
    spec = mc.OptionSpec(strike, t, mc.OptionType.Call if is_call else mc.OptionType.Put)
    return mc.black_scholes(spec, mc.MarketData(spot, R, vol, Q)).price


# ----------------------------------------------------- 1. value vs spot curve
spots = np.linspace(55, 145, 91)
value_curve = {
    "spot": spots.tolist(),
    "call": [bs_price(s, K, True) for s in spots],
    "put": [bs_price(s, K, False) for s in spots],
    "call_intrinsic": np.maximum(spots - K, 0.0).tolist(),
    "put_intrinsic": np.maximum(K - spots, 0.0).tolist(),
}

# -------------------------------------------------- 2. binomial tree convergence
tree_steps = list(range(3, 161))
tree_conv = {
    "steps": tree_steps,
    "price": [mc.binomial_tree(call, market, n).price for n in tree_steps],
    "truth": BS_TRUTH,
}

# ----------------------------------- 3. Monte Carlo convergence: pseudo vs Sobol
exps = list(range(10, 23))                 # 2^10 .. 2^22 paths
mc_paths = [1 << e for e in exps]
pseudo_err, sobol_err, pseudo_se = [], [], []
for n in mc_paths:
    # Expected pseudo-random error: RMS of |error| over many independent seeds,
    # which traces the characteristic O(1/sqrt(N)) trend cleanly. Sobol' is
    # deterministic, so a single run is its error.
    errs, se = [], 0.0
    for seed in range(24):
        r = mc.monte_carlo(call, market, paths=n, antithetic=True,
                           quasi_random=False, threads=0, seed=1000 + seed)
        errs.append(abs(r.price - BS_TRUTH))
        se = r.std_error
    pseudo_err.append(float(np.sqrt(np.mean(np.square(errs)))))
    pseudo_se.append(float(se))
    q = mc.monte_carlo(call, market, paths=n, antithetic=True,
                       quasi_random=True, threads=0)
    sobol_err.append(abs(q.price - BS_TRUTH))

# Guard against exact zeros for the log axis.
sobol_err = [max(e, 1e-7) for e in sobol_err]
pseudo_err = [max(e, 1e-7) for e in pseudo_err]
speedup = [p / s for p, s in zip(pseudo_err, sobol_err)]

mc_conv = {
    "paths": mc_paths,
    "exps": exps,
    "pseudo_err": pseudo_err,
    "sobol_err": sobol_err,
    "pseudo_se": pseudo_se,
    "truth": BS_TRUTH,
}

# ------------------------------------------------ 4. GBM path fan (the model)
rng = np.random.default_rng(7)
n_steps, n_paths = 50, 28
dt = T / n_steps
drift = (R - Q - 0.5 * VOL * VOL) * dt
vol_dt = VOL * math.sqrt(dt)
paths = np.empty((n_paths, n_steps + 1))
paths[:, 0] = S0
for k in range(1, n_steps + 1):
    z = rng.standard_normal(n_paths)
    paths[:, k] = paths[:, k - 1] * np.exp(drift + vol_dt * z)
gbm = {
    "t": np.linspace(0, T, n_steps + 1).tolist(),
    "curves": paths.tolist(),
    "mean": (S0 * np.exp((R - Q) * np.linspace(0, T, n_steps + 1))).tolist(),
    "strike": K,
}

# ----------------------------------------- 5. Sobol vs pseudo-random uniformity
N_UNIF = 256
sob = mc.sobol_points(N_UNIF)
pse = np.random.default_rng(3).random((N_UNIF, 2))
uniformity = {"sobol": sob.tolist(), "pseudo": pse.tolist(), "n": N_UNIF}

# ------------------------------------- 6. American: early-exercise premium
am_spots = np.linspace(70, 130, 61)
amer_put = mc.OptionSpec(K, T, mc.OptionType.Put, mc.OptionStyle.American)
amer_vals = [mc.binomial_tree(amer_put, mc.MarketData(s, R, VOL, Q), 800).price
             for s in am_spots]
euro_vals = [bs_price(s, K, False) for s in am_spots]
intrinsic = np.maximum(K - am_spots, 0.0)
# Early-exercise boundary at t=0: largest spot where holding == exercising.
boundary = None
for s, v in zip(am_spots, amer_vals):
    if v <= max(K - s, 0.0) + 1e-4 and s < K:
        boundary = float(s)
lsm = mc.longstaff_schwartz(amer_put, market, paths=200000, steps=50)
american = {
    "spot": am_spots.tolist(),
    "amer": amer_vals,
    "euro": euro_vals,
    "intrinsic": intrinsic.tolist(),
    "boundary": boundary,
    "lsm_price": lsm.price,
    "tree_price": mc.binomial_tree(amer_put, market, 2048).price,
    "euro_price": bs_put.price,
}

# --------------------------------------------- 7. throughput / thread scaling
hw = os.cpu_count() or 8
bench_paths = 1 << 23
thread_list = sorted({1, 2, 4, 6, 8, hw})


def time_run(threads):
    best = float("inf")
    for _ in range(3):
        t0 = time.perf_counter()
        mc.monte_carlo(call, market, paths=bench_paths, antithetic=True,
                       quasi_random=True, threads=threads)
        best = min(best, time.perf_counter() - t0)
    return best


mc.monte_carlo(call, market, paths=1 << 20, threads=hw)  # warm up
scaling_pps, scaling_ms = [], []
for t in thread_list:
    dt_s = time_run(t)
    scaling_ms.append(dt_s * 1e3)
    scaling_pps.append(bench_paths / dt_s)
scaling = {
    "threads": thread_list,
    "pps": scaling_pps,
    "time_ms": scaling_ms,
    "ideal": [scaling_pps[0] * t for t in thread_list],
    "paths": bench_paths,
}

# latency for a realistic 1M-path valuation at full width
t0 = time.perf_counter()
mc.monte_carlo(call, market, paths=1 << 20, quasi_random=True, antithetic=True,
               threads=hw)
latency_ms = (time.perf_counter() - t0) * 1e3

# ---------------------------------------- 8. cross-engine agreement table
def row(label, spec, market_, american_engine=False):
    bs = tree = mcv = lsmv = None
    if spec.style == mc.OptionStyle.European:
        bs = mc.black_scholes(spec, market_).price
        mcv = mc.monte_carlo(spec, market_, paths=1 << 21,
                             quasi_random=True, antithetic=True, threads=0).price
    tree = mc.binomial_tree(spec, market_, 2048).price
    if spec.style == mc.OptionStyle.American:
        lsmv = mc.longstaff_schwartz(spec, market_, paths=200000, steps=50).price
    return {"label": label, "bs": bs, "tree": tree, "mc": mcv, "lsm": lsmv}


engines_table = [
    row("ATM call", mc.OptionSpec(100, 1.0, mc.OptionType.Call), market),
    row("ITM call (K=90)", mc.OptionSpec(90, 1.0, mc.OptionType.Call), market),
    row("OTM put (K=90)", mc.OptionSpec(90, 1.0, mc.OptionType.Put), market),
    row("Long-dated call (T=2)", mc.OptionSpec(100, 2.0, mc.OptionType.Call), market),
    row("American put", mc.OptionSpec(100, 1.0, mc.OptionType.Put,
                                      mc.OptionStyle.American), market),
    row("American put (div 4%)",
        mc.OptionSpec(100, 1.0, mc.OptionType.Put, mc.OptionStyle.American),
        mc.MarketData(100, 0.05, 0.20, 0.04)),
]

# --------------------------------------------------------------- assemble
mae = sobol_err[-1]
data = {
    "meta": {
        "version": mc.__version__, "spot": S0, "strike": K, "rate": R,
        "vol": VOL, "maturity": T, "dividend": Q,
        "hw_threads": hw, "bench_paths": bench_paths,
    },
    "headline": {
        "throughput": max(scaling_pps),
        "latency_ms": latency_ms,
        "mae": mae,
        "speedup_max": max(speedup),
        "engines": 3,
    },
    "reference": {
        "bs": {"price": bs_call.price, "delta": bs_call.delta,
               "gamma": bs_call.gamma, "vega": bs_call.vega,
               "theta": bs_call.theta, "rho": bs_call.rho},
        "put_price": bs_put.price,
        "tree": mc.binomial_tree(call, market, 2048).price,
        "mc": {"price": mc.monte_carlo(call, market, paths=1 << 22,
                                       quasi_random=True, antithetic=True,
                                       threads=0).price},
    },
    "value_curve": value_curve,
    "tree_conv": tree_conv,
    "mc_conv": mc_conv,
    "gbm": gbm,
    "uniformity": uniformity,
    "american": american,
    "scaling": scaling,
    "engines_table": engines_table,
}

out = os.path.join(ROOT, "site", "data.json")
with open(out, "w") as f:
    json.dump(data, f, separators=(",", ":"))
print(f"wrote {out}")
print(f"  throughput {max(scaling_pps)/1e6:.1f}M paths/s | latency {latency_ms:.1f} ms"
      f" | mae {mae:.1e} | max speedup {max(speedup):.0f}x")
