/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/scores/old_bayesian.hpp"

#include "metrisca/core/logger.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"
#include "metrisca/models/model.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/core/parallel.hpp"
#include "metrisca/utils/template_attack.hpp"

#include <array>
#include <algorithm>
#include <tuple>
#include <mutex>
#include <unordered_set>

using namespace metrisca;

#define DOUBLE_NAN (std::numeric_limits<double>::signaling_NaN())
#define DOUBLE_INFINITY (std::numeric_limits<double>::infinity())
#define IS_VALID_DOUBLE(value) (std::isfinite(value) && !std::isnan(value))

static std::array<double, 256> computeProbabilities(
    const Matrix<int32_t>& modelizedTraces,
    const std::array<std::vector<size_t>, 256>& groupedByExpectedResult,
    const std::shared_ptr<TraceDataset>& dataset,
    uint32_t sampleStart,
    uint32_t sampleCount,
    uint32_t traceCount,
    uint32_t filterSampleCount
) {
    // For each group, computes the average within the group
    std::array<std::vector<double>, 256> averages; // [expected_result][sample]
    std::vector<bool> groupWithoutModel(256, false);

    for (size_t groupIdx = 0; groupIdx != 256; groupIdx++) {
        averages[groupIdx].resize(sampleCount, 0.0);
        size_t matching_trace_count = 0;

        for (size_t traceIdx : groupedByExpectedResult[groupIdx]) {
            // Ignore all traces outside of the sweet zone (aka. the traceCount)
            if (traceIdx >= traceCount) continue;
            matching_trace_count++;

            // Compute the average for this specific group
            for (size_t sampleIdx = sampleStart; sampleIdx != sampleStart + sampleCount; sampleIdx++) {
                averages[groupIdx][sampleIdx - sampleStart] += (double) dataset->GetSample(sampleIdx)[traceIdx];
            }
        }

        // If no traces matched, then set the average to NaN
        groupWithoutModel[groupIdx] = (matching_trace_count == 0);
        if (matching_trace_count == 0) {
            averages[groupIdx].assign(sampleCount, DOUBLE_NAN);
            continue;
        }
        else {
            for (size_t sampleIdx = 0; sampleIdx != sampleCount; sampleIdx++) {
                averages[groupIdx][sampleIdx] /= matching_trace_count;
            }
        }
    }

    // Reducing number of traces to speed up the computation
    std::vector<size_t> selectedSamples;
    std::vector<double> bestDiffPerSample;
    bestDiffPerSample.resize(sampleCount, 0.0);

    for (size_t i = 0; i != 256; ++i) {
        // Skip group without model
        if (groupWithoutModel[i]) continue;

        for (size_t j = i + 1; j != 256; ++j) {
            // Skip group without model
            if (groupWithoutModel[j]) continue;

            // Iterate over all available samples
            for (size_t sampleIdx = 0; sampleIdx != sampleCount; ++sampleIdx) {
                // Compute the difference between the two groups
                double diff = std::abs(averages[i][sampleIdx] - averages[j][sampleIdx]);

                // If the difference is better than the current best, then update the best
                if (bestDiffPerSample[sampleIdx] < diff) {
                    bestDiffPerSample[sampleIdx] = diff;
                }
            }
        }
    }

    // Because of numerical stability issues and optimization problems
    // we do not compute the covariance on all samples, only on the most
    // useful ones. To achieve this goal we will sort all sample by there best
    // diff, and select m_SampleFilterCount of the best sample
    {
        std::vector<size_t> reorderBuffer;
        reorderBuffer.reserve(sampleCount);
        for (size_t i = 0; i != 256; i++) {
            reorderBuffer.push_back(i);
        }

        std::sort(reorderBuffer.begin(), reorderBuffer.end(), [&](size_t lhs, size_t rhs) {
            return bestDiffPerSample[lhs] < bestDiffPerSample[rhs];
        });

        // Only keep filterSampleCount samples
        selectedSamples.assign(reorderBuffer.begin(), reorderBuffer.begin() + filterSampleCount);
    }

    // Conveniance variables
    size_t const reducedSampleCount = selectedSamples.size();

    // Compute the covariance matrix, and reset it to 0 by filling the rows
    Matrix<double> covariance(reducedSampleCount, reducedSampleCount);
    for (size_t i = 0; i != reducedSampleCount; ++i) {
        covariance.FillRow(i, 0.0);
    }

    for (size_t groupIdx = 0; groupIdx != 256; ++groupIdx)
    {
        for (size_t traceIdx : groupedByExpectedResult[groupIdx]) {
            if (traceIdx >= traceCount) continue;
            
            for (size_t row = 0; row != reducedSampleCount; row++) {
                for (size_t col = 0; col != reducedSampleCount; col++) {
                    covariance(row, col) += (dataset->GetSample(selectedSamples[row])[traceIdx] - averages[groupIdx][selectedSamples[row] - sampleStart]) *
                                            (dataset->GetSample(selectedSamples[col])[traceIdx] - averages[groupIdx][selectedSamples[col] - sampleStart]);
                }
            }
        }
    }

    for (size_t row = 0; row != reducedSampleCount; row++) {
        for (size_t col = 0; col != reducedSampleCount; col++) {
            covariance(row, col) /= (traceCount - 1);
        }
    }
    
    // Filter all "duplicated" samples (aka. samples with a correlation of 1 with another sample)
    std::vector<size_t> newSelectedSamples;
    Matrix<double> newCovariance;
    {
        std::vector<size_t> selectedSamplesIndexBuffer;
        selectedSamplesIndexBuffer.reserve(selectedSamples.size());

        for (size_t i = 0; i != selectedSamples.size(); ++i) {
            // Find whether or not we shall ignore this sample
            bool skip = covariance(i, i) < 1e-2;

            for (size_t j = 0; j != selectedSamplesIndexBuffer.size(); ++j) {
                double correlation = covariance(i, selectedSamplesIndexBuffer[j]) /
                                     std::sqrt(covariance(i, i) * covariance(selectedSamplesIndexBuffer[j], selectedSamplesIndexBuffer[j]));
                if (std::abs(correlation) > 1 - 1e-2) {
                    skip = true;
                    break;
                }
            }

            if (!skip) {
                selectedSamplesIndexBuffer.push_back(i);
            }
        }

        // Rebuild the selected samples list
        for (size_t idx : selectedSamplesIndexBuffer) {
            newSelectedSamples.push_back(selectedSamples[idx]);
        }

        // Rebuild the covariance matrix
        newCovariance.Resize(newSelectedSamples.size(), newSelectedSamples.size());
        for (size_t i = 0; i != newSelectedSamples.size(); ++i) {
            for (size_t j = 0; j != newSelectedSamples.size(); ++j) {
                newCovariance(i, j) = covariance(selectedSamplesIndexBuffer[i], selectedSamplesIndexBuffer[j]);
            }
        }
    }

    // Compute the inverse of the covariance matrix
    Matrix<double> inverseCovariance = newCovariance.CholeskyInverse();

    // Finally compute the probabilities
    std::array<double, 256> probabilities;
    std::vector<double> noiseVector;
    noiseVector.resize(newSelectedSamples.size(), 0.0);

    for (size_t groupIdx = 0; groupIdx != 256; groupIdx++)
    {
        probabilities[groupIdx] = 0.0;

        for (size_t traceIdx = 0; traceIdx != traceCount; ++traceIdx)
        {
            int32_t expectedOutput = modelizedTraces(groupIdx, traceIdx);

            // In case corresponding group does not exists
            if (groupWithoutModel[groupIdx]) {
                continue;
            }

            for (size_t j = 0; j != newSelectedSamples.size(); ++j)
            {
                noiseVector[j] = dataset->GetSample(newSelectedSamples[j])[traceIdx] - averages[expectedOutput][newSelectedSamples[j] - sampleStart];
            }

            // Compute probabilities
            for (size_t i = 0; i != newSelectedSamples.size(); ++i)
            {
                for (size_t j = 0; j != newSelectedSamples.size(); ++j)
                {
                    probabilities[groupIdx] += noiseVector[i] * inverseCovariance(i, j) * noiseVector[j];
                }
            }

            // Ensure the probabilities (log-likelihood) is negative
            probabilities[groupIdx] = -0.5 * probabilities[groupIdx];
        }
    }

    // Finally return the result of the operation
    return probabilities;
}


namespace metrisca
{
    Result<void, Error> OldBayesianPlugin::Init(const ArgumentList& args)
    {
        // Initialize the base score plugin
        auto base_init_result = ScorePlugin::Init(args);
        if (base_init_result.IsError()) {
            return base_init_result.Error();
        }

        // Retrieve the model
        {
            auto model_opt = args.GetString(ARG_NAME_MODEL);
            if (!model_opt.has_value()) {
                METRISCA_ERROR("Missing argument: {}", ARG_NAME_MODEL);
                return Error::INVALID_ARGUMENT;
            }

            // Try constructing the plugin
            auto plugin_or_error = PluginFactory::The().ConstructAs<PowerModelPlugin>(PluginType::PowerModel, model_opt.value(), args);
            if (plugin_or_error.IsError()) {
                METRISCA_ERROR("Failed to construct the model plugin: {}", model_opt.value());
                return plugin_or_error.Error();
            }

            m_Model = std::move(plugin_or_error.Value());
        }

        // Retrieve number of samples filter
        {
            auto sample_filter_opt = args.GetUInt32(ARG_NUMBER_SAMPLE_FILTER);
            if (!sample_filter_opt.has_value()) {
                METRISCA_ERROR("Missing argument: {}", ARG_NUMBER_SAMPLE_FILTER);
                return Error::INVALID_ARGUMENT;
            }
            m_SampleFilterCount = sample_filter_opt.value();
        }

        // Return success
        return {};
    }

    Result<std::vector<std::pair<uint32_t, std::vector<std::array<double, 256>>>>, Error>
    OldBayesianPlugin::ComputeScores()
    {
        // Conveniance variables
        size_t const byteCount = m_Dataset->GetHeader().KeySize;

        // Determines steps size
        std::vector<uint32_t> steps = (m_TraceStep > 0) ?
            numerics::ARange(m_TraceStep, m_TraceCount + 1, m_TraceStep) :
            std::vector<uint32_t>{ m_TraceCount };

        // Initialize the result
        std::vector<std::pair<uint32_t, std::vector<std::array<double, 256>>>> probabilities;
        probabilities.reserve(steps.size());
        for (size_t step : steps) {
            probabilities.emplace_back(step, std::vector<std::array<double, 256>>(byteCount));
        }

        // Modelize traces for each key byte
        std::vector<Matrix<int32_t>> modelizedTraces(byteCount);
        for (size_t byte = 0; byte != byteCount; ++byte) {
            m_Model->SetByteIndex(byte);
            auto model_or_error = m_Model->Model();
            if (model_or_error.IsError()) {
                METRISCA_ERROR("Failed to modelize traces for byte {}", byte);
                return model_or_error.Error();
            }
            modelizedTraces[byte] = std::move(model_or_error.Value());
        }

        // Using our prior knowledge of the correct key, group
        // the traces by their "expected" output using the model.
        std::vector<std::array<std::vector<size_t>, 256>> groupedByExpectedResult(byteCount);
        std::atomic_bool uniqueWarn = false;

        metrisca::parallel_for(0, byteCount, [&](size_t byteIdx) {
            // Group the traces
            for (size_t traceIdx = 0; traceIdx != m_TraceCount; ++traceIdx) {
                // Retrieve the expected result
                uint8_t const key = m_Dataset->GetKey(traceIdx)[byteIdx];
                int32_t expected_result = modelizedTraces[byteIdx](key, traceIdx);

                // If the expected_result is not in between [0, 255], then abort.
                if (expected_result < 0 || expected_result > 255) {
                    if (!uniqueWarn.exchange(true)) {
                        METRISCA_WARN("Expected result for byte {} is out of range: {}", byteIdx, expected_result);
                    }
                    continue;
                }

                // Group the trace
                groupedByExpectedResult[byteIdx][expected_result].push_back(traceIdx);
            }
        });

        // For each key byte and each step
        metrisca::parallel_for(0, byteCount * steps.size(), [&](size_t idx) {
            // Retrieve the byte and step
            size_t byteIdx = idx / steps.size();
            size_t stepIdx = idx % steps.size();

            // Retrieve the group probabilities
            probabilities[stepIdx].second[byteIdx] = computeProbabilities(
                modelizedTraces[byteIdx],
                groupedByExpectedResult[byteIdx],
                m_Dataset,
                m_SampleStart,
                m_SampleCount,
                steps[stepIdx],
                m_SampleFilterCount
            );
        });

        // Return the result
        return probabilities;
    }
}
