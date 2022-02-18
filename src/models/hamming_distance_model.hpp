/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _HAMMING_DISTANCE_MODEL_HPP
#define _HAMMING_DISTANCE_MODEL_HPP

#include "forward.hpp"

namespace metrisca {

    namespace models {

        /**
         * Compute the hamming distance model. Only S-BOX is currently supported.
         */
        int HammingDistanceModel(Matrix<int>& out, const TraceDataset& dataset, unsigned int byte_index);
    }

}

#endif