/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _MODEL_HPP
#define _MODEL_HPP

#include "forward.hpp"

#include <functional>

namespace metrisca {

    /**
     * Represent a function that computes a power model for a given dataset.
     * The arguments are defined in order as follows:
     * 1) The matrix that will contain the values of the power model. The number of
     *    rows should match the number of keys in the model and the number of column
     *    should match the number of plaintexts in the model.
     *    This is an output value
     * 2) The input dataset object.
     * 3) The byte index to target. The bounds to this value depend on the algorithm used. E.g., S-BOX uses
     *    one-byte keys and plaintexts, therefore, the byte index can only be 0.
     * The return value of this function is an element of the error enumeration.
     */
    typedef std::function<int(Matrix<int>&, const TraceDataset&, unsigned int)> ModelFunction;

}

#endif