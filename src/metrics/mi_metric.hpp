/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _MI_METRIC_HPP
#define _MI_METRIC_HPP

#include "forward.hpp"

namespace metrisca {

    namespace metrics {

        /**
         * Compute the mutual information metric for a specific key
         */
        int MIMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context);
    }

}

#endif