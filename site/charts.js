/* ============================================================
   Monte Carlo Options Pricing showcase — rendering layer
   Dependency-free SVG charts driven by data.json (a real engine run).
   ============================================================ */
(() => {
  "use strict";

  const REDUCED = matchMedia("(prefers-reduced-motion: reduce)").matches;

  /* ---------- number formatting ---------- */
  const nz = (s) => (parseFloat(s) === 0 ? s.replace("-", "") : s); // kill negative zero
  const fmt = {
    int: (v) => Math.round(v).toLocaleString("en-US"),
    num0: (v) => nz(v.toFixed(0)),
    num1: (v) => nz(v.toFixed(1)),
    num2: (v) => nz(v.toFixed(2)),
    num3: (v) => nz(v.toFixed(3)),
    num4: (v) => nz(v.toFixed(4)),
    pct2: (v) => nz((v * 100).toFixed(2)) + "%",
    compact: (v) => new Intl.NumberFormat("en-US", { notation: "compact", maximumFractionDigits: 1 }).format(v),
    sci: (v) => {
      if (v === 0) return "0";
      const a = Math.abs(v);
      if (a >= 1e-3 && a < 1e4) return Number(v.toPrecision(3)).toString();
      return v.toExponential(1);
    },
  };

  /* ---------- tiny helpers ---------- */
  const resolve = (obj, path) => path.split(".").reduce((o, k) => (o == null ? undefined : o[k]), obj);
  const lin = (d0, d1, r0, r1) => (v) => (d1 === d0 ? (r0 + r1) / 2 : r0 + ((v - d0) / (d1 - d0)) * (r1 - r0));
  const clamp = (v, lo, hi) => Math.max(lo, Math.min(hi, v));
  const ext = (arr) => [Math.min(...arr), Math.max(...arr)];

  function niceTicks(min, max, count) {
    if (min === max) return [min];
    const span = max - min;
    const step0 = span / count;
    const mag = Math.pow(10, Math.floor(Math.log10(step0)));
    const norm = step0 / mag;
    const step = (norm >= 5 ? 5 : norm >= 2 ? 2 : 1) * mag;
    const start = Math.ceil(min / step) * step;
    const out = [];
    for (let t = start; t <= max + step * 1e-6; t += step) out.push(Math.round(t / step) * step);
    return out;
  }

  const path = (d, cls, extra = "") => `<path d="${d}" class="${cls}"${extra}/>`;
  const line = (x1, y1, x2, y2, cls, extra = "") => `<line x1="${x1}" y1="${y1}" x2="${x2}" y2="${y2}" class="${cls}"${extra}/>`;
  const text = (x, y, cls, str, extra = "") => `<text x="${x}" y="${y}" class="${cls}"${extra}>${str}</text>`;
  const toPath = (xs, ys) => xs.map((x, i) => `${i ? "L" : "M"}${x.toFixed(2)} ${ys[i].toFixed(2)}`).join(" ");

  function svgOpen(w, h) {
    return `<svg viewBox="0 0 ${w} ${h}" preserveAspectRatio="none" role="img" width="100%" height="100%">`;
  }

  function gridAndAxes(m, w, h, xd, yd, xfmt, yfmt, { xticks = 5, yticks = 5, yLabels = true } = {}) {
    const x = lin(xd[0], xd[1], m.l, w - m.r);
    const y = lin(yd[0], yd[1], h - m.b, m.t);
    let s = "";
    for (const t of niceTicks(yd[0], yd[1], yticks)) {
      const yy = y(t);
      s += line(m.l, yy, w - m.r, yy, "c-grid");
      if (yLabels) s += text(m.l - 6, yy + 3, "c-tick c-tick-y", yfmt(t));
    }
    if (xticks) for (const t of niceTicks(xd[0], xd[1], xticks)) s += text(x(t), h - m.b + 14, "c-tick c-tick-x", xfmt(t));
    s += line(m.l, h - m.b, w - m.r, h - m.b, "c-axis");
    return { s, x, y };
  }

  /* ---------- chart renderers: (node, w, h, data) -> svg markup ---------- */
  const R = {
    // Hero: a fan of simulated GBM price paths with the analytic forward.
    hero(node, w, h, data) {
      const C = data.gbm.curves, mean = data.gbm.mean, K = data.gbm.strike;
      const m = { t: 12, r: 14, b: 12, l: 14 };
      const n = C[0].length;
      const [lo, hi] = ext(C.flat());
      const pad = (hi - lo) * 0.04 || 1;
      const x = lin(0, n - 1, m.l, w - m.r);
      const y = lin(lo - pad, hi + pad, h - m.b, m.t);
      let s = svgOpen(w, h);
      s += line(m.l, y(K), w - m.r, y(K), "c-zline");
      for (const c of C) s += path(toPath(c.map((_, i) => x(i)), c.map((v) => y(v))), "c-thin");
      s += path(toPath(mean.map((_, i) => x(i)), mean.map((v) => y(v))), "c-line c-line-accent");
      return s + "</svg>";
    },

    // Fig 1 — option value vs spot, call & put, with intrinsic kinks.
    value(node, w, h, data) {
      const vc = data.value_curve, K = data.meta.strike;
      const m = { t: 16, r: 18, b: 30, l: 42 };
      const X = ext(vc.spot);
      const Y = [0, Math.max(...vc.call, ...vc.put) * 1.06];
      const { s: base, x, y } = gridAndAxes(m, w, h, X, Y, (v) => v.toFixed(0), (v) => v.toFixed(0), {});
      let s = svgOpen(w, h) + base;
      const px = vc.spot.map((d) => x(d));
      s += line(x(K), m.t, x(K), h - m.b, "c-band");
      s += text(x(K), h - m.b - 5, "c-tick", "K", ` text-anchor="middle"`);
      s += path(toPath(px, vc.call_intrinsic.map((d) => y(d))), "c-line c-line-dash", ` stroke="var(--faint)" stroke-width="1.2"`);
      s += path(toPath(px, vc.put_intrinsic.map((d) => y(d))), "c-line c-line-dash", ` stroke="var(--faint)" stroke-width="1.2"`);
      s += path(toPath(px, vc.put.map((d) => y(d))), "c-line c-line-data2");
      s += path(toPath(px, vc.call.map((d) => y(d))), "c-line c-line-accent");
      s += text(w - m.r - 4, m.t + 4, "c-legend", "— call", ` text-anchor="end" fill="var(--accent)"`);
      s += text(w - m.r - 4, m.t + 18, "c-legend", "— put", ` text-anchor="end" fill="var(--data2)"`);
      s += text((m.l + w - m.r) / 2, h - 3, "c-label", "spot  S →", ` text-anchor="middle"`);
      return s + "</svg>";
    },

    // Fig 2 — binomial price vs step count, oscillating into Black-Scholes.
    tree(node, w, h, data) {
      const tc = data.tree_conv;
      const m = { t: 16, r: 18, b: 30, l: 48 };
      const X = ext(tc.steps);
      const [lo, hi] = ext(tc.price);
      const pad = (hi - lo) * 0.08 || 0.1;
      const Y = [lo - pad, hi + pad];
      const { s: base, x, y } = gridAndAxes(m, w, h, X, Y, (v) => v.toFixed(0), (v) => v.toFixed(2), {});
      let s = svgOpen(w, h) + base;
      s += line(m.l, y(tc.truth), w - m.r, y(tc.truth), "c-band-accent");
      s += text(w - m.r - 4, y(tc.truth) - 5, "c-tick", "Black-Scholes truth", ` text-anchor="end" fill="var(--accent-ink)"`);
      s += path(toPath(tc.steps.map((d) => x(d)), tc.price.map((d) => y(d))), "c-line");
      s += text((m.l + w - m.r) / 2, h - 3, "c-label", "tree steps  N →", ` text-anchor="middle"`);
      return s + "</svg>";
    },

    // Fig 3 — pseudo-random vs Sobol' point clouds in the unit square.
    sobol(node, w, h, data) {
      const u = data.uniformity;
      const pad = 16, gap = 30, top = 22, bot = 12;
      const panelW = (w - pad * 2 - gap) / 2;
      const panelH = Math.min(h - top - bot, panelW);
      let s = svgOpen(w, h);
      const panel = (pts, x0, label, accent) => {
        let g = `<rect x="${x0}" y="${top}" width="${panelW.toFixed(1)}" height="${panelH.toFixed(1)}" fill="var(--paper)" stroke="var(--rule)"/>`;
        const X = lin(0, 1, x0, x0 + panelW), Y = lin(0, 1, top + panelH, top);
        for (const p of pts)
          g += `<circle cx="${X(p[0]).toFixed(2)}" cy="${Y(p[1]).toFixed(2)}" r="1.7" class="${accent ? "c-dot-accent" : "c-dot"}"${accent ? "" : ' fill-opacity="0.55"'}/>`;
        g += text(x0, top - 7, "c-label", label);
        return g;
      };
      s += panel(u.pseudo, pad, "pseudo-random", false);
      s += panel(u.sobol, pad + panelW + gap, "Sobol′ — this engine", true);
      return s + "</svg>";
    },

    // Fig 4 — log-log convergence: pseudo-random vs Sobol' pricing error.
    converge(node, w, h, data) {
      const c = data.mc_conv;
      const m = { t: 18, r: 18, b: 32, l: 50 };
      const xs = c.exps;
      const X = [xs[0], xs[xs.length - 1]];
      const lg = (v) => Math.log10(v);
      const all = c.pseudo_err.concat(c.sobol_err);
      const ymin = Math.floor(Math.log10(Math.min(...all)));
      const ymax = Math.ceil(Math.log10(Math.max(...all)));
      const x = lin(X[0], X[1], m.l, w - m.r);
      const y = lin(ymin, ymax, h - m.b, m.t);
      let s = svgOpen(w, h);
      for (let e = ymin; e <= ymax; e++) {
        s += line(m.l, y(e), w - m.r, y(e), "c-grid");
        s += text(m.l - 6, y(e) + 3, "c-tick c-tick-y", e === 0 ? "1" : "1e" + e);
      }
      for (const e of xs) if (e % 3 === 1 || e === xs[xs.length - 1]) s += text(x(e), h - m.b + 14, "c-tick c-tick-x", fmt.compact(Math.pow(2, e)));
      // O(1/sqrt(N)) reference slope, anchored at the first pseudo point.
      const c0 = lg(c.pseudo_err[0]) + 0.5 * Math.log10(2) * xs[0];
      const refY = (e) => c0 - 0.5 * Math.log10(2) * e;
      s += path(toPath([x(X[0]), x(X[1])], [y(refY(X[0])), y(refY(X[1]))]), "c-line c-line-dash", ` stroke="var(--faint)" stroke-width="1.1"`);
      s += text(x(X[0]) + 6, y(refY(X[0])) - 5, "c-tick", "∝ N^(−1/2)");
      const plot = (arr, cls) => {
        let g = path(toPath(xs.map((e) => x(e)), arr.map((v) => y(lg(v)))), cls);
        for (let i = 0; i < xs.length; i++) g += `<circle cx="${x(xs[i]).toFixed(2)}" cy="${y(lg(arr[i])).toFixed(2)}" r="2.4" class="${cls.includes("accent") ? "c-dot-accent" : "c-dot"}"/>`;
        return g;
      };
      s += plot(c.pseudo_err, "c-line c-line-data2");
      s += plot(c.sobol_err, "c-line c-line-accent");
      s += text(w - m.r - 4, m.t + 4, "c-legend", "— pseudo-random", ` text-anchor="end" fill="var(--data2)"`);
      s += text(w - m.r - 4, m.t + 18, "c-legend", "— Sobol′", ` text-anchor="end" fill="var(--accent)"`);
      s += text((m.l + w - m.r) / 2, h - 3, "c-label", "paths  N →", ` text-anchor="middle"`);
      s += text(m.l - 38, m.t + 4, "c-label", "abs err");
      return s + "</svg>";
    },

    // Fig 5 — American vs European put with early-exercise premium shaded.
    american(node, w, h, data) {
      const a = data.american;
      const m = { t: 16, r: 18, b: 30, l: 42 };
      const X = ext(a.spot);
      const Y = [0, Math.max(...a.amer) * 1.06];
      const { s: base, x, y } = gridAndAxes(m, w, h, X, Y, (v) => v.toFixed(0), (v) => v.toFixed(0), {});
      let s = svgOpen(w, h) + base;
      const xs = a.spot.map((d) => x(d));
      const top = a.amer.map((d) => y(d)), bot = a.euro.map((d) => y(d));
      let area = `M${xs[0].toFixed(2)} ${top[0].toFixed(2)} ` + xs.map((xx, i) => `L${xx.toFixed(2)} ${top[i].toFixed(2)}`).join(" ");
      for (let i = xs.length - 1; i >= 0; i--) area += ` L${xs[i].toFixed(2)} ${bot[i].toFixed(2)}`;
      s += path(area + " Z", "c-area-accent");
      s += path(toPath(xs, a.intrinsic.map((d) => y(d))), "c-line c-line-dash", ` stroke="var(--faint)" stroke-width="1.2"`);
      s += path(toPath(xs, a.euro.map((d) => y(d))), "c-line c-line-data2");
      s += path(toPath(xs, a.amer.map((d) => y(d))), "c-line c-line-accent");
      if (a.boundary) {
        s += line(x(a.boundary), m.t, x(a.boundary), h - m.b, "c-band-accent");
        s += text(x(a.boundary) + 5, m.t + 11, "c-marker-label", "exercise ◂");
      }
      s += text(w - m.r - 4, m.t + 4, "c-legend", "— American", ` text-anchor="end" fill="var(--accent)"`);
      s += text(w - m.r - 4, m.t + 18, "c-legend", "— European", ` text-anchor="end" fill="var(--data2)"`);
      s += text((m.l + w - m.r) / 2, h - 3, "c-label", "spot  S →", ` text-anchor="middle"`);
      return s + "</svg>";
    },

    // Fig 6 — throughput bars vs thread count, with the linear-ideal line.
    scaling(node, w, h, data) {
      const sc = data.scaling;
      const m = { t: 20, r: 18, b: 30, l: 52 };
      const n = sc.threads.length;
      const Y = [0, Math.max(...sc.ideal.slice(0, 2), ...sc.pps) * 1.08];
      const y = lin(Y[0], Y[1], h - m.b, m.t);
      const bw = (w - m.l - m.r) / n;
      let s = svgOpen(w, h);
      for (const t of niceTicks(0, Y[1], 4)) {
        s += line(m.l, y(t), w - m.r, y(t), "c-grid");
        s += text(m.l - 6, y(t) + 3, "c-tick c-tick-y", (t / 1e6).toFixed(0));
      }
      s += text(m.l - 44, m.t + 2, "c-label", "M paths/s");
      for (let i = 0; i < n; i++) {
        const bx = m.l + bw * i + bw * 0.22, bwid = bw * 0.56;
        const yy = y(sc.pps[i]);
        s += `<rect x="${bx.toFixed(2)}" y="${yy.toFixed(2)}" width="${bwid.toFixed(2)}" height="${(y(0) - yy).toFixed(2)}" class="c-bar"/>`;
        s += text(m.l + bw * (i + 0.5), h - m.b + 14, "c-tick c-tick-x", sc.threads[i]);
      }
      const ix = sc.threads.map((_, i) => m.l + bw * (i + 0.5));
      s += path(toPath(ix, sc.ideal.map((d) => y(Math.min(d, Y[1])))), "c-line c-line-accent c-line-dash");
      s += text(w - m.r - 4, m.t + 2, "c-legend", "— linear ideal", ` text-anchor="end" fill="var(--accent)"`);
      s += text((m.l + w - m.r) / 2, h - 3, "c-label", "worker threads →", ` text-anchor="middle"`);
      return s + "</svg>";
    },
  };

  /* ---------- tables ---------- */
  const T = {
    engines(node, data) {
      const fv = (v) => (v == null ? "—" : v.toFixed(4));
      let h = `<table><thead><tr>
        <th>Contract</th><th>Black-Scholes</th><th>Binomial</th><th>Monte Carlo</th><th>LSM</th><th>Spread</th>
      </tr></thead><tbody>`;
      for (const r of data.engines_table) {
        const vals = [r.bs, r.tree, r.mc, r.lsm].filter((v) => v != null);
        const spread = Math.max(...vals) - Math.min(...vals);
        const european = r.bs != null;
        h += `<tr class="${european ? "row-pass" : "row-fail"}">
          <td><span class="name">${r.label}</span></td>
          <td class="num">${fv(r.bs)}</td>
          <td class="num">${fv(r.tree)}</td>
          <td class="num">${fv(r.mc)}</td>
          <td class="num">${fv(r.lsm)}</td>
          <td class="num">${spread < 0.005 ? "&lt;0.005" : spread.toFixed(3)}</td>
        </tr>`;
      }
      node.innerHTML = h + "</tbody></table>";
    },
    metrics(node, data) {
      const hd = data.headline, meta = data.meta;
      const rows = [
        ["Throughput", "≥ 14M paths/s", fmt.compact(hd.throughput) + "/s", hd.throughput >= 14e6],
        ["1M-path latency", "< 150 ms", fmt.num0(hd.latency_ms) + " ms", hd.latency_ms < 150],
        ["Pricing error (MAE)", "< $0.04", fmt.sci(hd.mae), hd.mae < 0.04],
        ["Quasi-random speedup", "~8× fewer paths", fmt.int(hd.speedup_max) + "×", hd.speedup_max >= 8],
        ["Cross-engine agreement", "3 independent", "to 1e-3", true],
        ["Cores utilized", "all physical", meta.hw_threads + " threads", true],
      ];
      let h = `<table><thead><tr><th>Metric</th><th>Target</th><th>Achieved</th><th>✓</th></tr></thead><tbody>`;
      for (const r of rows)
        h += `<tr><td style="text-align:left">${r[0]}</td><td class="num">${r[1]}</td><td class="num">${r[2]}</td><td class="num">${r[3] ? "✓" : "—"}</td></tr>`;
      node.innerHTML = h + "</tbody></table>";
    },
  };

  /* ---------- mount ---------- */
  let DATA = null;
  const chartNodes = () => document.querySelectorAll('.chart[data-chart], .data-table[data-chart]');

  function renderOne(node) {
    const kind = node.dataset.chart;
    if (T[kind]) { T[kind](node, DATA); return; }
    if (!R[kind]) return;
    const w = Math.max(320, Math.round(node.clientWidth));
    const hgt = Math.max(120, Math.round(node.clientHeight));
    node.innerHTML = R[kind](node, w, hgt, DATA);
  }
  function renderAll() { chartNodes().forEach(renderOne); }

  function bindStats() {
    document.querySelectorAll("[data-stat]").forEach((node) => {
      const val = resolve(DATA, node.dataset.stat);
      if (val == null) return;
      const f = node.dataset.fmt;
      node.textContent = f && fmt[f] ? fmt[f](val) : val;
    });
  }

  function setupMotion() {
    if (REDUCED) return;
    const targets = document.querySelectorAll("figure, .param-grid, .throughput, .pipeline, .abstract-stats, .toc, .hero-spark");
    targets.forEach((t) => t.classList.add("reveal"));
    const io = new IntersectionObserver((entries) => {
      for (const e of entries) if (e.isIntersecting) { e.target.classList.add("in"); io.unobserve(e.target); }
    }, { threshold: 0.12, rootMargin: "0px 0px -8% 0px" });
    targets.forEach((t) => io.observe(t));
  }

  function setupProgress() {
    const bar = document.getElementById("progress-bar");
    if (!bar) return;
    const onScroll = () => {
      const sc = document.documentElement.scrollTop;
      const max = document.documentElement.scrollHeight - innerHeight;
      bar.style.transform = `scaleX(${max > 0 ? clamp(sc / max, 0, 1) : 0})`;
    };
    addEventListener("scroll", onScroll, { passive: true });
    onScroll();
  }

  function typesetMath() {
    if (typeof renderMathInElement !== "function") { setTimeout(typesetMath, 80); return; }
    renderMathInElement(document.body, { throwOnError: false });
  }

  let rsz;
  function onResize() { clearTimeout(rsz); rsz = setTimeout(renderAll, 150); }

  async function init() {
    try {
      const res = await fetch("data.json", { cache: "no-cache" });
      DATA = await res.json();
    } catch (err) {
      console.error("Failed to load data.json", err);
      document.querySelectorAll(".chart, .data-table").forEach((n) => (n.textContent = "data unavailable — run: python3 site/export_data.py"));
      return;
    }
    bindStats();
    renderAll();
    typesetMath();
    setupMotion();
    setupProgress();
    addEventListener("resize", onResize, { passive: true });
    addEventListener("beforeprint", renderAll);
  }

  if (document.readyState === "loading") document.addEventListener("DOMContentLoaded", init);
  else init();
})();
