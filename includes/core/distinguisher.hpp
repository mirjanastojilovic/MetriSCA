/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _DISTINGUISHER_HPP
#define _DISTINGUISHER_HPP

#include "forward.hpp"

#include <functional>
#include <vector>

namespace metrisca {

    /**
     * Represent a distinguisher function.
     * The arguments are defined in order as follows:
     * 1) List of distinguisher output. The first element of the pair contains the number of 
     *    traces that were used, the second element contains a matrix with the same number of rows
     *    as the number of keys and the same number of columns as the number of samples that were analysed.
     *    This is an output parameter.
     * 2) Input dataset object.
     * 3) Matrix containing the computed model.
     * 4) Index of the first sample to analyse.
     * 5) Non-inclusive index of the last sample to analyse.
     * 6) Maximum number of traces to use during analysis.
     * 7) The trace step. The function should apply the distinguisher with an increasing number of traces
     *    starting from this value until the maximum number of traces is reached. This value defines the
     *    step size of this iteration.
     * The return value of this function is an element of the error enumeration.
     */
    typedef std::function<int(
        std::vector<std::pair<uint32_t, Matrix<double>>>&, const TraceDataset&, const Matrix<int>&, uint32_t, uint32_t, uint32_t, uint32_t)> DistinguisherFunction;

}

#endif