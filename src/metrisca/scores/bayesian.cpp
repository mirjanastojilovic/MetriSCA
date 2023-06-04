/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/scores/bayesian.hpp"

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

// Approach :
// 1. Profile the dataset
//   - For each sample, and each traces, compute the correlation with the 
//     expected value. Once this correlation is known it is assumes that
//     each key byte is leaked at different samples. Therefore we then find
//     for each key byte, a subset of samples that leaks it. 
//   - We find the linear correction factory (bias + variance) for every traces
//     based on these few samples
// 2. Attack the dataset
//   - Finally compute the noise vector along with the covariance matrix in order
//     to approximate the distribution


/**
 * @brief Linear correction factor
 * 
 * This struct defines all parameters for a simple linear correction factor (with bias).
 * Both parameters can be computed using @ref LinearCorrectionFactorAccumulator.
 */
struct LinearCorrectionFactor
{
    double alpha;
    double beta;

    double operator()(uint8_t value) const
    {
        return alpha * ((double) value) + beta;
    }
};

/**
 * @brief Accumulator for the linear correction factor
 * 
 * This helper structure is used to compute the linear correction factor
 * that minimizes the mean square error between the expected value and the
 * actual value.
 */
struct LinearCorrectionFactorAccumulator
{
    double uSum;
    double vSum;
    double u2Sum;
    double uvSum;
    double v2Sum;
    double N;

    LinearCorrectionFactorAccumulator() { reset(); }

    void reset() {
        uSum = 0.0;
        vSum = 0.0;
        u2Sum = 0.0;
        uvSum = 0.0;
        v2Sum = 0.0;
        N = 0.0;
    }

    void accumulate(double u, double v) {
        uSum += u;
        vSum += v;
        u2Sum += u * u;
        uvSum += u * v;
        v2Sum += v * v;
        N += 1.0;
    }

    void accumulateConcurrently(LinearCorrectionFactorAccumulator const& acc, std::mutex& globalLock) {
        std::lock_guard<std::mutex> guard(globalLock);
        uSum += acc.uSum;
        vSum += acc.vSum;
        u2Sum += acc.u2Sum;
        v2Sum += acc.v2Sum;
        uvSum += acc.uvSum;
        N += acc.N;
    }

    LinearCorrectionFactor build() const {
        LinearCorrectionFactor result;
        result.alpha = (N * uvSum - vSum) / (N * u2Sum - uSum * uSum);
        result.beta = (vSum - result.alpha * uSum) / N;
        return result;
    }
};


struct ProfiledResult
{
    std::vector<std::vector<size_t>> selectedSamples;
    std::vector<LinearCorrectionFactor> correctionFactors;
};

static Result<ProfiledResult, Error> profile(
    std::shared_ptr<TraceDataset> dataset,
    std::shared_ptr<PowerModelPlugin> model,
    size_t sampleStart, 
    size_t sampleCount,
    size_t selectedSampleCount
) {
    // Conveniance variables
    size_t const traceCount = dataset->GetHeader().NumberOfTraces;
    size_t const sampleEnd = sampleStart + sampleCount;
    size_t const byteCount = dataset->GetHeader().KeySize;
    ProfiledResult result;
    result.selectedSamples.resize(byteCount);
    result.correctionFactors.resize(sampleCount);

    // Modelize the side channel
    METRISCA_TRACE("Modelizing the side channel");
    model->SetDataset(dataset);
    std::vector<Matrix<int32_t>> models;
    models.resize(byteCount);

    for (size_t byteIdx = 0; byteIdx != byteCount; ++byteIdx) {
        model->SetByteIndex(byteIdx);
        auto model_or_error = model->Model();
        if (model_or_error.IsError()) {
            return model_or_error.Error();
        }
        models[byteIdx] = std::move(model_or_error.Value());
    }

    // In a second time, try to find the samples that leaks the most by computing
    // the correlation between the expected value and the actual value for each samples
    METRISCA_TRACE("Finding the samples that leaks the most (best correlation)");
    metrisca::parallel_for(0, byteCount, [&](size_t byteIdx) {
        // Compute the correlation 
        std::vector<double> correlations;
        correlations.resize(sampleCount);

        // For each sample
        for (size_t sampleIdx = 0; sampleIdx != sampleCount; ++sampleIdx) {
            auto sample = dataset->GetSample(sampleIdx + sampleStart);

            // Compute the correlation
            double uSum = 0.0;
            double vSum = 0.0;
            double uvSum = 0.0;
            double u2Sum = 0.0;
            double v2Sum = 0.0;
            size_t N = (size_t) traceCount;

            for (size_t traceIdx = 0; traceIdx != traceCount; ++traceIdx) {
                double u = sample[traceIdx];
                double v = models[byteIdx](dataset->GetKey(traceIdx)[byteIdx], traceIdx);

                uSum += u;
                vSum += v;
                uvSum += u * v;
                u2Sum += u * u;
                v2Sum += v * v;
            }

            correlations[sampleIdx] = (N * uvSum - uSum * vSum);
            correlations[sampleIdx] /= std::sqrt(N * u2Sum - uSum * uSum) * std::sqrt(N * v2Sum - vSum * vSum);
        }

        // Sort the correlation through a reorder buffer
        std::vector<size_t> reorderBuffer;
        reorderBuffer.reserve(sampleCount);
        for (size_t i = 0; i != sampleCount; ++i) reorderBuffer.push_back(i);
        // std::iota(reorderBuffer.begin(), reorderBuffer.end(), (size_t) 0);
        std::sort(reorderBuffer.begin(), reorderBuffer.end(), [&](size_t lhs, size_t rhs) {
            if (std::isnan(correlations[lhs])) return false;
            if (std::isnan(correlations[rhs])) return true;
            return correlations[lhs] > correlations[rhs];
        });

        // Select the samples that "leaks" the most (positive correlation)
        result.selectedSamples[byteIdx].resize(selectedSampleCount);
        std::copy(reorderBuffer.begin(), reorderBuffer.begin() + selectedSampleCount, result.selectedSamples[byteIdx].begin());
        std::transform(result.selectedSamples[byteIdx].begin(), result.selectedSamples[byteIdx].end(), result.selectedSamples[byteIdx].begin(), [sampleStart](size_t idx) {
            return idx + sampleStart;
        });
    });

    // Compute the linear correction factor for each sample
    METRISCA_TRACE("Computing the linear correction factor for each sample");
    std::vector<LinearCorrectionFactorAccumulator> accumulators;
    accumulators.resize(sampleCount);
    std::mutex globalLock;

    metrisca::parallel_for(0, byteCount * selectedSampleCount, [&](size_t idx)
    {
        // Compute the byte index and the sample index
        size_t byteIdx = idx / selectedSampleCount;
        size_t sampleIdx = idx % selectedSampleCount;

        // Compute the correction factor
        LinearCorrectionFactorAccumulator accumulator;
        if (sampleIdx >= result.selectedSamples[byteIdx].size()) {
            return;
        }

        auto samples = dataset->GetSample(result.selectedSamples[byteIdx][sampleIdx]);
        for (size_t traceIdx = 0; traceIdx != traceCount; ++traceIdx) {
            double u = samples[traceIdx];
            double v = models[byteIdx](dataset->GetKey(traceIdx)[byteIdx], traceIdx);
            accumulator.accumulate(u, v);
        }

        // Store the correction factor (notice that this operation is thread-safe
        // due to global lock)
        accumulators[result.selectedSamples[byteIdx][sampleIdx]].accumulateConcurrently(accumulator, globalLock);
    });

    // Build the correction factors for each samples
    METRISCA_TRACE("Building the correction factors for each samples");
    std::transform(accumulators.begin(), accumulators.end(), result.correctionFactors.begin(), [](LinearCorrectionFactorAccumulator const& acc) {
        return acc.build();
    });

    // Return the result
    return result;
}


namespace metrisca
{
    Result<void, Error> BayesianPlugin::Init(const ArgumentList& args)
    {
        // Initialize the base score plugin
        auto base_init_result = ScorePlugin::Init(args);
        if (base_init_result.IsError()) {
            return base_init_result.Error();
        }

        // Retrieve the profiling dataset
        {
            auto profiling_dataset_opt = args.GetDataset(ARG_NAME_TRAINING_DATASET);
            if (!profiling_dataset_opt.has_value()) {
                METRISCA_ERROR("Missing argument: {}", ARG_NAME_TRAINING_DATASET);
                return Error::INVALID_ARGUMENT;
            }
            m_ProfilingDataset = profiling_dataset_opt.value();
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

        // Sanity check
        if (m_ProfilingDataset->GetHeader().NumberOfSamples != m_Dataset->GetHeader().NumberOfSamples ||
            m_ProfilingDataset->GetHeader().KeySize != m_Dataset->GetHeader().KeySize ||
            m_ProfilingDataset->GetHeader().PlaintextSize != m_Dataset->GetHeader().PlaintextSize ||
            m_ProfilingDataset->GetHeader().EncryptionType != m_Dataset->GetHeader().EncryptionType) {
            METRISCA_ERROR("Both the profiling and the attack dataset must match");
            return Error::INVALID_ARGUMENT;
        }

        // Return success
        return {};
    }

    Result<std::vector<std::pair<uint32_t, std::vector<std::array<double, 256>>>>, Error>
    BayesianPlugin::ComputeScores()
    {
        // Conveniance variables
        size_t const byteCount = m_Dataset->GetHeader().KeySize;

        // Determines steps size
        std::vector<uint32_t> steps = (m_TraceStep > 0) ?
            numerics::ARange(m_TraceStep, m_TraceCount + 1, m_TraceStep) :
            std::vector<uint32_t>{ m_TraceCount };

        // Profile the dataset
        METRISCA_TRACE("Profiling the dataset");
        ProfiledResult profiledResult;
        {
            auto profile_or_error = profile(m_ProfilingDataset, m_Model, m_SampleStart, m_SampleCount, m_SampleFilterCount);
            if (profile_or_error.IsError()) {
                return profile_or_error.Error();
            }
            profiledResult = std::move(profile_or_error.Value());
        }

        // Modelize the attack
        METRISCA_INFO("Modelizing the attack");
        std::vector<Matrix<int32_t>> modelsPerKeyByte;
        modelsPerKeyByte.resize(byteCount);

        m_Model->SetDataset(m_Dataset);
        for (size_t keyByteIdx = 0; keyByteIdx != byteCount; ++keyByteIdx) {
            m_Model->SetByteIndex(keyByteIdx);
            auto model_or_error = m_Model->Model();
            if (model_or_error.IsError()) {
                return model_or_error.Error();
            }
            modelsPerKeyByte[keyByteIdx] = std::move(model_or_error.Value());
        }

        // Begin of the attack phase
        METRISCA_INFO("Attack phase");
        std::vector<std::pair<uint32_t, std::vector<std::array<double, 256>>>> scores;
        scores.reserve(steps.size());
        for (size_t step : steps) scores.push_back(std::make_pair(step, std::vector<std::array<double, 256>>(byteCount)));

        // Compute probabilities (log)
        metrisca::parallel_for("Computing log probabilities ", 0, steps.size() * byteCount, [&](size_t idx){
            // Compute the step and the key byte index
            size_t stepIdx = idx / byteCount;
            size_t keyByteIdx = idx % byteCount;

            // Find all the samples that are selected for this key byte
            std::vector<size_t> selectedSamples = profiledResult.selectedSamples[keyByteIdx];

            // For each key hypothesis
            for (size_t key = 0; key != 256; key++) {
                std::vector<double> noise;
                noise.reserve(selectedSamples.size());
                
                for (size_t sampleIdx : selectedSamples) {
                    auto samples = m_Dataset->GetSample(sampleIdx);
                    double noiseSample = 0.0;

                    for (size_t traceIdx = 0; traceIdx != steps[stepIdx]; ++traceIdx) {
                        double u = samples[traceIdx];
                        double v = modelsPerKeyByte[keyByteIdx](key, traceIdx);
                        noiseSample += profiledResult.correctionFactors[sampleIdx](u) - v;
                    }

                    noise.push_back(noiseSample / steps[stepIdx]);
                }

                // Compute the covariance matrix
                Matrix<double> covarianceMatrix(selectedSamples.size(), selectedSamples.size());
                
                for (size_t i = 0; i != selectedSamples.size(); ++i) {
                    for (size_t j = 0; j != selectedSamples.size(); ++j) {
                        double uSum = 0.0;
                        double vSum = 0.0;
                        double uvSum = 0.0;

                        for (size_t traceIdx = 0; traceIdx != steps[stepIdx]; ++traceIdx) {
                            double u = profiledResult.correctionFactors[selectedSamples[i]](m_Dataset->GetSample(selectedSamples[i])[traceIdx])
                                - modelsPerKeyByte[keyByteIdx](key, traceIdx);
                            double v = profiledResult.correctionFactors[selectedSamples[j]](m_Dataset->GetSample(selectedSamples[j])[traceIdx])
                                - modelsPerKeyByte[keyByteIdx](key, traceIdx);
                            uSum += u;
                            vSum += v;
                            uvSum += u * v;
                        }

                        covarianceMatrix(i, j) = (uvSum - uSum * vSum / steps[stepIdx]) / (steps[stepIdx] - 1);
                    }
                }

                // If the covariance matrix is not full-rank then reduced number of selectedSamples
                std::vector<size_t> selectedSamplesIndexes;
                selectedSamplesIndexes.reserve(selectedSamples.size());
                for (size_t i = 0; i != selectedSamples.size(); ++i) {
                    bool is_repeated = false;

                    for (size_t j = 0; j != selectedSamplesIndexes.size(); ++j) {
                        // Compute the correlation between both samples
                        double correlation = covarianceMatrix(i, selectedSamplesIndexes[j]) /
                            std::sqrt(covarianceMatrix(i, i) * covarianceMatrix(selectedSamplesIndexes[j], selectedSamplesIndexes[j]));
                        
                        if (std::abs(correlation) > .98) {
                            is_repeated = true;
                            break;
                        }
                    }

                    if (!is_repeated) {
                        selectedSamplesIndexes.push_back(i);
                    }
                }

                // Filter the noise vector and the covariance matrix
                Matrix<double> reducedCovarianceMatrix(selectedSamplesIndexes.size(), selectedSamplesIndexes.size());
                for (size_t i = 0; i != selectedSamplesIndexes.size(); ++i) {
                    for (size_t j = 0; j != selectedSamplesIndexes.size(); ++j) {
                        reducedCovarianceMatrix(i, j) = covarianceMatrix(selectedSamplesIndexes[i], selectedSamplesIndexes[j]);
                    }
                }
                
                // Invert the covariance matrix
                Matrix<double> inverseCovarianceMatrix = reducedCovarianceMatrix.CholeskyInverse();

                // Finally compute the probability
                double probability = 0.0;
                for (size_t i = 0; i != selectedSamplesIndexes.size(); ++i) {
                    for (size_t j = 0; j != selectedSamplesIndexes.size(); ++j) {
                        probability += noise[selectedSamplesIndexes[i]] * 
                                       inverseCovarianceMatrix(i, j) *
                                       noise[selectedSamplesIndexes[j]];
                    }
                }
                scores[stepIdx].second[keyByteIdx][key] = -0.5 * probability;
            }
        });

        // Return the result
        return scores;
    }
}
