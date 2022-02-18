/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _GUESSING_ENTROPY_METRIC_HPP
#define _GUESSING_ENTROPY_METRIC_HPP

#include "forward.hpp"

namespace metrisca {

    namespace metrics {

        /**
         * Compute the guessing entropy of the correct key in bits.
         * This value correspond to the log2 of the rank of the correct key.
         */
        int GuessingEntropyMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context);
    }

}

#endif