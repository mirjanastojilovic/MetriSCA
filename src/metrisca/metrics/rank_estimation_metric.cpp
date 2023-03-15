/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "rank_estimation_metric.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"

namespace metrisca {

    Result<void, Error> RankEstimationMetric::Compute() 
    {
        // Retrieve the pearson correlation
        auto score_or_error = m_Distinguisher->Distinguish();
        if (score_or_error.IsError())
            return score_or_error.Error();

        // Find the maximum 
    }

}
