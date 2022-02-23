/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/guessing_entropy_metric.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"

namespace metrisca {

    Result<void, int> GuessingEntropyMetric::Init(const ArgumentList& args)
    {
        auto base_result = BasicMetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result.Error();

        auto known_key = args.GetUInt8(ARG_NAME_KNOWN_KEY);

        if(!known_key.has_value())
            return SCA_MISSING_ARGUMENT;

        m_KnownKey = known_key.value();

        return {};
    }

    Result<void, int> GuessingEntropyMetric::Compute()
    {
        auto score_or_error = m_Distinguisher->Distinguish();
        if(score_or_error.IsError())
            return score_or_error.Error();

        auto score = score_or_error.Value();

        // Write CSV header
        CSVWriter writer(m_OutputFile);
        writer << "trace_count";
        writer << "logrank_key" + std::to_string(m_KnownKey);
        writer << csv::EndRow;

        // Write CSV data
        // First column is number of traces, then correct key logrank
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

            std::vector<uint32_t> ranks;
            ranks.resize(256);
            for(uint32_t i = 0; i < key_max_scores.size(); i++)
            {
                ranks[key_max_scores[i].first] = i+1;
            }

            writer << std::log2((double)ranks[m_KnownKey]);

            writer << csv::EndRow;
        }

        return {};
    }

}