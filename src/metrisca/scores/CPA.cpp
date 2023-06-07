/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/scores/CPA.hpp"

#include "metrisca/core/logger.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"
#include "metrisca/models/model.hpp"
#include "metrisca/utils/numerics.hpp"

namespace metrisca {

    Result<void, Error> CPAPlugin::Init(const ArgumentList& args)
    {
        // Initialize base plugin
        {
            auto result = ScorePlugin::Init(args);
            if (result.IsError())
                return result.Error();
        }

        // Retrieve distinguisher plugin
        auto distinguisher_string = args.GetString(ARG_NAME_DISTINGUISHER);
        if (!distinguisher_string.has_value()) {
            METRISCA_ERROR("Missing argument: {}", ARG_NAME_DISTINGUISHER);
            return Error::MISSING_ARGUMENT;
        }

        // Construct distinguisher plugin
        auto result = PluginFactory::The().ConstructAs<DistinguisherPlugin>(PluginType::Distinguisher, distinguisher_string.value(), args);
        if (result.IsError()) {
            METRISCA_ERROR("Failed to construct distinguisher plugin: {}", distinguisher_string.value());
            return result.Error();
        }

        m_Distinguisher = std::move(result.Value());

        // Return success
        return {};
    }


    Result<std::vector<std::pair<uint32_t, std::vector<std::array<double, 256>>>>, Error> CPAPlugin::ComputeScores()
    {
        // Enumerate each steps (number of traces)
        std::vector<uint32_t> steps = (m_TraceStep > 0) ?
            numerics::ARange(m_TraceStep, m_TraceCount + 1, m_TraceStep) :
            std::vector<uint32_t>{ m_TraceCount };

        // For each key-byte, each key value
        std::vector<std::pair<uint32_t, std::vector<std::array<double, 256>>>> scores;
        scores.resize(steps.size());

        // For each key-byte and each key find best correlation
        for (size_t keyByte = 0; keyByte != m_Dataset->GetHeader().KeySize; ++keyByte) {
            m_Distinguisher->GetPowerModel()->SetByteIndex(keyByte);

            // Distiguish each step
            std::vector<std::pair<uint32_t, metrisca::Matrix<double>>> result;

            {
                auto partialResult = m_Distinguisher->Distinguish();
                if (partialResult.IsError()) {
                    METRISCA_ERROR("Failed to distinguish key byte {}", keyByte);
                    return partialResult.Error();
                }
                result = std::move(partialResult.Value());
            }

            // For each key candidate find best correlation
            std::array<double, 256> bestCorrelations;
            for (size_t stepIdx = 0; stepIdx != result.size(); ++stepIdx) {
                const auto& pearsonCorrelatioMatrix = result[stepIdx].second;
                bestCorrelations.fill(0.0);

                for (size_t key = 0; key != 256; key++) {
                    for (size_t sampleIdx = 0; sampleIdx != pearsonCorrelatioMatrix.GetHeight(); sampleIdx++) {
                        bestCorrelations[key] = std::log(std::max(bestCorrelations[key], std::abs(pearsonCorrelatioMatrix(sampleIdx, key))));
                    }
                }

                // Finally, store the best correlation for each key candidate
                scores[stepIdx].second.push_back(bestCorrelations);
                scores[stepIdx].first = steps[stepIdx];
            }
        }

        // Return success
        return scores;
    }

}

