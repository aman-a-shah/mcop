// rng.hpp — sources of standard-normal N(0,1) draws for the simulation engine.
//
// A small polymorphic interface lets the Monte Carlo core be written once and
// fed either pseudo-random numbers (this file) or low-discrepancy Sobol
// sequences (added later) without touching the pricing loops.
#pragma once

#include <cstdint>
#include <random>

namespace mcop {

// Abstract source of independent standard-normal variates.
class NormalStream {
public:
    virtual ~NormalStream() = default;
    // Fill `out[0..n)` with N(0,1) draws.
    virtual void fill(double* out, std::size_t n) = 0;
};

// Mersenne-Twister pseudo-random normal stream. Each instance owns its own
// engine state so distinct instances (e.g. one per worker thread) are
// independent and lock-free.
class PseudoNormalStream : public NormalStream {
public:
    explicit PseudoNormalStream(std::uint64_t seed = 0x9E3779B97F4A7C15ULL)
        : engine_(seed) {}

    void fill(double* out, std::size_t n) override {
        for (std::size_t i = 0; i < n; ++i) out[i] = dist_(engine_);
    }

    double next() { return dist_(engine_); }

private:
    std::mt19937_64 engine_;
    std::normal_distribution<double> dist_{0.0, 1.0};
};

}  // namespace mcop
