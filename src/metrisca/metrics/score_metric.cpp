/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/score_metric.hpp"

#include "metrisca/core/csv_writer.hpp"
#include "metrisca/core/errors.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"

#include <vector>
#include <string>
#include <array>

namespace metrisca {

    Result<void, Error> ScoreMetric::Compute()
    {
        auto score_or_error = m_Distinguisher->Distinguish();
        if(score_or_error.IsError())
            return score_or_error.Error();

        auto score = score_or_error.Value();

        // Write CSV header
        CSVWriter writer(m_OutputFile);
        writer << "trace_count";
        for(uint32_t i = 0; i < 256; ++i)
        {
            writer << "score_key" + std::to_string(i);
        }
        writer << csv::EndRow;

        // Write CSV data
        // First column is number of traces, then key max scores
        for(size_t step = 0; step < score.size(); step++)
        {
            uint32_t stepCount = score[step].first;
            writer << stepCount;

            const Matrix<double>& scores = score[step].second;
            for(uint32_t k = 0; k < 256; k++)
            {
                const nonstd::span<const double> keyScore = scores.GetRow(k);
                double maxScore = numerics::Max(keyScore);
                writer << maxScore;
            }

            writer << csv::EndRow;
        }

        return {};
    }

}