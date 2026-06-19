// math_utils.hpp — small, fast numerical primitives shared across engines.
#pragma once

#include <cmath>

namespace mcop {
namespace math {

constexpr double kPi = 3.14159265358979323846;
constexpr double kInvSqrt2Pi = 0.3989422804014327;  // 1 / sqrt(2*pi)
constexpr double kInvSqrt2 = 0.7071067811865476;     // 1 / sqrt(2)

// Standard normal probability density function.
inline double norm_pdf(double x) {
    return kInvSqrt2Pi * std::exp(-0.5 * x * x);
}

// Standard normal cumulative distribution function.
//
// Implemented branchlessly via std::erf for accuracy and to avoid pipeline
// stalls from data-dependent branches in tight pricing loops:
//   N(x) = 0.5 * (1 + erf(x / sqrt(2)))
inline double norm_cdf(double x) {
    return 0.5 * std::erfc(-x * kInvSqrt2);
}

}  // namespace math
}  // namespace mcop
