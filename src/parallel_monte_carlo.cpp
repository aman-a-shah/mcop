#include "mcop/parallel_monte_carlo.hpp"

#include "mcop/gbm_sample.hpp"
#include "mcop/mc_kernel.hpp"
#include "mcop/thread_pool.hpp"

#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace mcop {

PricingResult monte_carlo_european_parallel(const OptionSpec& spec,
                                            const MarketData& market,
                                            const MonteCarloConfig& cfg,
                                            unsigned num_threads, ThreadPool* pool) {
    if (cfg.num_paths == 0) throw std::invalid_argument("num_paths must be > 0");

    if (spec.maturity <= 0.0) {
        PricingResult res{};
        res.price = spec.payoff(market.spot);
        return res;
    }

    const detail::GbmParams p = detail::GbmParams::from(spec, market);
    const std::size_t total =
        cfg.antithetic ? (cfg.num_paths + 1) / 2 : cfg.num_paths;

    // Resolve worker count.
    unsigned hw = std::thread::hardware_concurrency();
    if (hw == 0) hw = 1;
    unsigned workers = num_threads ? num_threads : (pool ? pool->size() : hw);
    if (workers == 0) workers = 1;
    workers = static_cast<unsigned>(std::min<std::size_t>(workers, total));

    // Single worker: run inline (avoids pool overhead, keeps small jobs fast).
    if (workers <= 1) {
        detail::Accumulator acc;
        detail::run_mc_block(p, cfg, 0, total, cfg.seed, acc);
        return detail::finalize_mc(acc);
    }

    // Partition the statistical samples into contiguous, disjoint blocks.
    const std::size_t base = total / workers;
    const std::size_t rem = total % workers;

    auto run_chunk = [&](unsigned w) {
        const std::size_t start = w * base + std::min<std::size_t>(w, rem);
        const std::size_t count = base + (w < rem ? 1 : 0);
        detail::Accumulator acc;
        // Each pseudo-random worker gets an independent, well-separated seed;
        // Sobol' workers skip-ahead via `start` inside run_mc_block.
        const std::uint64_t seed = cfg.seed + static_cast<std::uint64_t>(w) *
                                                   0x9E3779B97F4A7C15ULL;
        detail::run_mc_block(p, cfg, start, count, seed, acc);
        return acc;
    };

    detail::Accumulator total_acc;

    // Use the caller's pool if supplied; otherwise spin up local threads.
    std::unique_ptr<ThreadPool> local;
    if (!pool) local = std::make_unique<ThreadPool>(workers);
    ThreadPool& tp = pool ? *pool : *local;

    std::vector<std::future<detail::Accumulator>> futs;
    futs.reserve(workers);
    for (unsigned w = 0; w < workers; ++w) {
        futs.emplace_back(tp.enqueue([run_chunk, w] { return run_chunk(w); }));
    }
    for (auto& f : futs) total_acc.combine(f.get());

    return detail::finalize_mc(total_acc);
}

}  // namespace mcop
