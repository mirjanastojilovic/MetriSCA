/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _MATH_HPP
#define _MATH_HPP

#include <cmath>

#define _INV_SQRT_2PI 0.39894228040143267793994605993438

namespace metrisca { namespace math {

    /// Compute the output of the gaussian function
    static inline double Gaussian(double x, double mean, double invstd) 
    {
        double centered = x - mean;
        double exp = centered * invstd;
        return _INV_SQRT_2PI * invstd * std::exp(-0.5 * exp * exp);
    }

}}

#endif