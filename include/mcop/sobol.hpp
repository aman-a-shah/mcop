// sobol.hpp — Sobol' low-discrepancy sequence generator + normal stream.
//
// Sobol' points fill the unit hypercube far more uniformly than pseudo-random
// draws, pushing Monte Carlo integration error from O(1/sqrt(N)) toward the
// quasi-random limit of ~O(1/N). Direction numbers follow the classic
// Numerical Recipes table (supports up to 6 dimensions), generated via the
// Gray-code recurrence.
#pragma once

#include "mcop/inverse_normal.hpp"
#include "mcop/rng.hpp"

#include <array>
#include <cstdint>
#include <stdexcept>

namespace mcop {

class SobolGenerator {
public:
    static constexpr int kMaxBit = 30;
    static constexpr int kMaxDim = 6;

    explicit SobolGenerator(int dim);

    // Fill x[0..dim) with the next point's coordinates, each in (0,1).
    void next(double* x);

    int dimension() const { return dim_; }

private:
    int dim_;
    std::uint32_t count_ = 0;
    double fac_;
    std::array<std::uint32_t, kMaxDim + 1> ix_{};
    std::array<std::uint32_t, kMaxDim * kMaxBit + 1> iv_{};
};

// Standard-normal stream backed by Sobol' points and an inverse-CDF transform.
// `dim` is the problem dimension (e.g. number of time steps per path); each
// emitted block of `dim` normals comes from one Sobol' point.
class SobolNormalStream : public NormalStream {
public:
    explicit SobolNormalStream(int dim = 1) : gen_(dim), dim_(dim) {}

    void fill(double* out, std::size_t n) override {
        std::array<double, SobolGenerator::kMaxDim> pt{};
        std::size_t i = 0;
        while (i < n) {
            gen_.next(pt.data());
            const std::size_t take = std::min<std::size_t>(dim_, n - i);
            for (std::size_t j = 0; j < take; ++j) {
                double p = pt[j];
                if (p <= 0.0) p = 1e-16;
                else if (p >= 1.0) p = 1.0 - 1e-16;
                out[i + j] = math::inv_norm_cdf(p);
            }
            i += take;
        }
    }

private:
    SobolGenerator gen_;
    int dim_;
};

}  // namespace mcop
