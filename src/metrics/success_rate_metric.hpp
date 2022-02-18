/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _SUCCESS_RATE_METRIC_HPP
#define _SUCCESS_RATE_METRIC_HPP

#include "forward.hpp"

namespace metrisca {

    namespace metrics {

        /**
         * Compute the binary success rate of the correct key. The value is equal to 1 if the key
         * was correctly guessed and false otherwise.
         */
        int SuccessRateMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context);
    }

}

#endif