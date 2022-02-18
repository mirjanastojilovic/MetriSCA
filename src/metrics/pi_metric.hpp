/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _PI_METRIC_HPP
#define _PI_METRIC_HPP

#include "forward.hpp"

namespace metrisca {

    namespace metrics {

        /**
         * Compute the perceived information metric for a specific key
         */
        int PIMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context);
    }

}

#endif