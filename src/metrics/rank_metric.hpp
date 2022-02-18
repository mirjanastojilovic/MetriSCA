/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _RANK_METRIC_HPP
#define _RANK_METRIC_HPP

#include "forward.hpp"

namespace metrisca {

    namespace metrics {

        /**
         * Compute the rank of each key. A rank of 1 means that the key was correctly guessed.
         */
        int RankMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context);
    }

}

#endif