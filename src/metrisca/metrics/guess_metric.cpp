/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/guess_metric.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"

namespace metrisca {

    Result<void, Error> GuessMetric::Compute()
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
            writer << "key_guess" + std::to_string(i+1);
        }
        writer << csv::EndRow;

        // Write CSV data
        // First column is number of traces, then key guess
        for(size_t step = 0; step < score.size(); step++)
        {
            uint32_t stepCount = score[step].first;
            writer << stepCount;

            const Matrix<double>& scores = score[step].second;
            std::vector<std::pair<uint32_t, double>> key_max_scores;
            key_max_scores.reserve(256);
            for(uint32_t k = 0; k < 256; k++)
            {
                const nonstd::span<const double> keyScore = scores.GetRow(k);
                double maxScore = numerics::Max(keyScore);
                key_max_scores.emplace_back(k, maxScore);
            }
            std::sort(key_max_scores.begin(), key_max_scores.end(), [](auto& a, auto& b) 
            {
                return a.second > b.second;
            });

            for(const auto& score : key_max_scores)
            {
                writer << score.first;
            }

            writer << csv::EndRow;
        }

        return {};
    }

}