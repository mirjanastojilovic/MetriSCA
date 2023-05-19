/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/rank_enumeration_metric.hpp"

#include "metrisca/core/logger.hpp"
#include "metrisca/core/errors.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"
#include "metrisca/models/model.hpp"
#include "metrisca/core/lazy_function.hpp"
#include "metrisca/core/indicators.hpp"
#include "metrisca/core/arg_list.hpp"
#include "metrisca/core/parallel.hpp"

#include <limits>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <math.h>


namespace metrisca {

    Result<void, Error> RankEnumerationMetric::Init(const ArgumentList& args)
    {
        // First initialize the base plugin
        auto base_result = BasicMetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result.Error();

        // Ensure the dataset has fixed key
        if (m_Dataset->GetHeader().KeyMode != KeyGenerationMode::FIXED) {
            METRISCA_ERROR("RankEnumerationMetric require the key to be fixed accross the entire dataset");
            return Error::UNSUPPORTED_OPERATION;
        }

        // Retrieve the number of enumerated key count
        auto enumerated_key_count = args.GetUInt32(ARG_NAME_ENUMERATED_KEY_COUNT);
        if (enumerated_key_count.has_value()) {
            return Error::INVALID_ARGUMENT;
        }
        m_EnumeratedKeyCount = enumerated_key_count.value();

        // Retrieve the key from the dataset
        auto key_span = m_Dataset->GetKey(0);
        m_Key.insert(m_Key.end(), key_span.begin(), key_span.end());

        // Finally return the success of the initialization of the current plugin
        return {};
    }

    Result<void, Error> RankEnumerationMetric::Compute()
    {
        // First compute the score for each key for each key byte
        METRISCA_INFO("Computing score(s) for each key and key byte");
        std::unordered_map<uint32_t, std::vector<std::array<double, 256>>> scoresPerSteps;
        {
            auto result = ComputeScores();
            if (result.IsError()) {
                return result.Error();
            }
            scoresPerSteps = std::move(result.Value());
        }

        // Secondly for each step, enumerate all possible keys
        std::vector<uint32_t> steps(scoresPerSteps.size());
        std::transform(scoresPerSteps.begin(), scoresPerSteps.end(), steps.begin(), [](auto x) { return x.first; });

        metrisca::parallel_for(0, scoresPerSteps.size(), [&](size_t idx)
        {
            // Retrieve the score corresponding with the current step
            const std::vector<std::array<double, 256>>& scores = scoresPerSteps[steps[idx]];

            
        });

        return {};
    }

    Result<std::unordered_map<uint32_t, std::vector<std::array<double, 256>>>, Error>
    RankEnumerationMetric::ComputeScores()
    {
        std::unordered_map<uint32_t, std::vector<std::array<double, 256>>> scoresPerSteps;

        for (size_t keyByteIdx = 0; keyByteIdx != m_Key.size(); ++keyByteIdx) {
            m_Distinguisher->GetPowerModel()->SetByteIndex(keyByteIdx);

            METRISCA_INFO("Run distinguisher for key byte {}", keyByteIdx);
            auto result = m_Distinguisher->Distinguish();
            if (result.IsError()) {
                METRISCA_ERROR("RankEnumerationMetric failed to compute scores for key byte {}", keyByteIdx);
                return result.Error();
            }

            if (keyByteIdx == 0) {
                for (auto [steps, matrix] : result.Value()) {
                    std::vector<std::array<double, 256>> v;
                    v.resize(16);
                    scoresPerSteps.insert(std::make_pair(steps, std::move(v)));
                }
            }

            for (auto [steps, matrix] : result.Value()) {
                
                for (size_t key = 0; key != 256; key++) {
                    double score = 0.0; ///TODO: Do the score thingy

                    scoresPerSteps[steps][keyByteIdx][key] = score;
                }

            }
        }

        return scoresPerSteps;
    }

}
