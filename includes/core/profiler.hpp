/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _PROFILER_HPP
#define _PROFILER_HPP

#include "forward.hpp"

#include <functional>

namespace metrisca {

    /**
     * Represent a function that can profile a dataset for a particular key.
     * The arguments are defined in order as follows:
     * 1) Matrix that will contain the profiling results. It should have the same
     *    number of columns as there are keys (usually 256). The first row contains the
     *    mean and the second row should contain the standard deviation of the associated
     *    normal distribution.
     * 2) The key to target. This is a number in the range [0,255]
     * 3) The byte index to target. The bounds to this value depend on the algorithm used. E.g., S-BOX uses
     *    one-byte keys and plaintexts, therefore, the byte index can only be 0.
     * The return value of this function is an element of the error enumeration.
     */
    typedef std::function<int(Matrix<double>&, const TraceDataset&, unsigned char, unsigned int)> ProfilerFunction;

}

#endif