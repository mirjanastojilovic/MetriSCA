/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef PEARSON_DISTINGUISHER_HPP
#define PEARSON_DISTINGUISHER_HPP

#include "forward.hpp"

#include <vector>

namespace metrisca {

    namespace distinguishers {

        /**
         * \brief Implement the Pearson distinguisher used by CPA.
         */
        int PearsonDistinguisher(std::vector<std::pair<unsigned int, Matrix<double>>>& out, 
                            const TraceDataset& dataset, 
                            const Matrix<int>& model, 
                            unsigned int sample_start, 
                            unsigned int sample_end, 
                            unsigned int trace_count, 
                            unsigned int trace_step);
    }

}

#endif