/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _STANDARD_PROFILER
#define _STANDARD_PROFILER

#include "forward.hpp"

namespace metrisca {

    namespace profilers {

        /**
         * Compute a profile on a dataset for a specific key. A profile is composed of
         * the mean and std of a Gaussian distribution of leakage values for a specific class.
         * Only the S-box is currently supported.
         */
        int StandardProfiler(Matrix<double>& out, const TraceDataset& dataset, unsigned char key, unsigned int byte_index);
    }

}

#endif