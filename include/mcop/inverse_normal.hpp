// inverse_normal.hpp — inverse standard-normal CDF (quantile function).
//
// Acklam's rational approximation with one Halley refinement step, giving full
// double precision (|error| < 1e-15). Used to convert low-discrepancy uniform
// Sobol points into standard-normal variates.
#pragma once

#include "mcop/math_utils.hpp"

#include <cmath>

namespace mcop {
namespace math {

inline double inv_norm_cdf(double p) {
    // Coefficients for Acklam's algorithm.
    static const double a[6] = {-3.969683028665376e+01, 2.209460984245205e+02,
                                -2.759285104469687e+02, 1.383577518672690e+02,
                                -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[5] = {-5.447609879822406e+01, 1.615858368580409e+02,
                                -1.556989798598866e+02, 6.680131188771972e+01,
                                -1.328068155288572e+01};
    static const double c[6] = {-7.784894002430293e-03, -3.223964580411365e-01,
                                -2.400758277161838e+00, -2.549732539343734e+00,
                                4.374664141464968e+00, 2.938163982698783e+00};
    static const double d[4] = {7.784695709041462e-03, 3.224671290700398e-01,
                                2.445134137142996e+00, 3.754408661907416e+00};

    const double p_low = 0.02425;
    const double p_high = 1.0 - p_low;
    double x;

    if (p < p_low) {
        const double qv = std::sqrt(-2.0 * std::log(p));
        x = (((((c[0] * qv + c[1]) * qv + c[2]) * qv + c[3]) * qv + c[4]) * qv + c[5]) /
            ((((d[0] * qv + d[1]) * qv + d[2]) * qv + d[3]) * qv + 1.0);
    } else if (p <= p_high) {
        const double qv = p - 0.5;
        const double rr = qv * qv;
        x = (((((a[0] * rr + a[1]) * rr + a[2]) * rr + a[3]) * rr + a[4]) * rr + a[5]) * qv /
            (((((b[0] * rr + b[1]) * rr + b[2]) * rr + b[3]) * rr + b[4]) * rr + 1.0);
    } else {
        const double qv = std::sqrt(-2.0 * std::log(1.0 - p));
        x = -(((((c[0] * qv + c[1]) * qv + c[2]) * qv + c[3]) * qv + c[4]) * qv + c[5]) /
             ((((d[0] * qv + d[1]) * qv + d[2]) * qv + d[3]) * qv + 1.0);
    }

    // One Halley step polishes the approximation to full precision.
    const double e = norm_cdf(x) - p;
    const double u = e * 2.5066282746310002 * std::exp(0.5 * x * x);  // e / pdf(x)
    x = x - u / (1.0 + 0.5 * x * u);
    return x;
}

}  // namespace math
}  // namespace mcop
