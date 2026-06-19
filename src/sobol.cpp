#include "mcop/sobol.hpp"

namespace mcop {

SobolGenerator::SobolGenerator(int dim) : dim_(dim) {
    if (dim < 1 || dim > kMaxDim) {
        throw std::invalid_argument("SobolGenerator supports dimensions 1..6");
    }

    // Primitive-polynomial degrees and coefficients (1-based, index 0 unused).
    static const std::uint32_t mdeg[kMaxDim + 1] = {0, 1, 2, 3, 3, 4, 4};
    static const std::uint32_t ip[kMaxDim + 1] = {0, 0, 1, 1, 2, 1, 4};
    // Seed direction numbers for the first `mdeg[k]` bits of each dimension.
    static const std::uint32_t iv_seed[] = {0, 1, 1, 1, 1, 1, 1, 3, 1, 3, 3, 1, 1,
                                            5, 7, 7, 3, 3, 5, 15, 11, 5, 15, 13, 9};

    for (std::size_t i = 0; i < iv_.size(); ++i) {
        iv_[i] = (i < sizeof(iv_seed) / sizeof(iv_seed[0])) ? iv_seed[i] : 0u;
    }
    ix_.fill(0u);
    fac_ = 1.0 / static_cast<double>(1u << kMaxBit);

    // iu(j,k) aliases iv_[(j-1)*kMaxDim + k]; build the full direction-number
    // table via the recurrence driven by each dimension's primitive polynomial.
    auto iu = [&](int j, int k) -> std::uint32_t& {
        return iv_[(j - 1) * kMaxDim + k];
    };
    for (int k = 1; k <= kMaxDim; ++k) {
        for (int j = 1; j <= static_cast<int>(mdeg[k]); ++j) {
            iu(j, k) <<= (kMaxBit - j);
        }
        for (int j = mdeg[k] + 1; j <= kMaxBit; ++j) {
            std::uint32_t ipp = ip[k];
            std::uint32_t val = iu(j - mdeg[k], k);
            val ^= (val >> mdeg[k]);
            for (int l = mdeg[k] - 1; l >= 1; --l) {
                if (ipp & 1u) val ^= iu(j - l, k);
                ipp >>= 1;
            }
            iu(j, k) = val;
        }
    }
}

void SobolGenerator::next(double* x) {
    // Gray-code update: find the position of the lowest zero bit of the counter.
    std::uint32_t im = count_++;
    int j = 1;
    for (; j <= kMaxBit; ++j) {
        if (!(im & 1u)) break;
        im >>= 1;
    }
    const std::uint32_t base = static_cast<std::uint32_t>(j - 1) * kMaxDim;
    for (int k = 1; k <= dim_; ++k) {
        ix_[k] ^= iv_[base + k];
        x[k - 1] = ix_[k] * fac_;
    }
}

}  // namespace mcop
