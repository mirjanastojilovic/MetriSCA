/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/rank_estimation_metric.hpp"

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
#include <math.h>

#define DOUBLE_NAN (std::numeric_limits<double>::signaling_NaN())

namespace metrisca {

    Result<void, Error> RankEstimationMetric::Init(const ArgumentList& args)
    {
        // First initialize the base plugin
        auto base_result = BasicMetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result.Error();

        // Ensure the dataset has fixed key
        if (m_Dataset->GetHeader().KeyMode != KeyGenerationMode::FIXED) {
            METRISCA_ERROR("RankEstimationMetric require the key to be fixed accross the entire dataset");
            return Error::UNSUPPORTED_OPERATION;
        }

        // Retrieve the key from the dataset
        auto key_span = m_Dataset->GetKey(0);
        m_Key.insert(m_Key.end(), key_span.begin(), key_span.end());

        // Retrieve the bin count argument
        auto bin_count = args.GetUInt32(ARG_NAME_BIN_COUNT);
        m_BinCount = bin_count.value_or(10000);
        

        // Sample & trace count/step
        auto sample_start = args.GetUInt32(ARG_NAME_SAMPLE_START);
        auto sample_end = args.GetUInt32(ARG_NAME_SAMPLE_END);
        auto trace_count = args.GetUInt32(ARG_NAME_TRACE_COUNT);
        auto trace_step = args.GetUInt32(ARG_NAME_TRACE_STEP);
        TraceDatasetHeader header = m_Dataset->GetHeader();
        m_TraceCount = trace_count.value_or(header.NumberOfTraces);
        m_TraceStep = trace_step.value_or(0);
        m_SampleStart = sample_start.value_or(0);
        m_SampleCount = sample_end.value_or(header.NumberOfSamples) - m_SampleStart;

        // Sanity checks
        if(m_SampleCount == 0)
            return Error::INVALID_ARGUMENT;

        if(m_SampleStart + m_SampleCount > header.NumberOfSamples)
            return Error::INVALID_ARGUMENT;

        if(m_TraceCount > header.NumberOfTraces)
            return Error::INVALID_ARGUMENT;

        return  {};
    }

    Result<void, Error> RankEstimationMetric::Compute() 
    {
        // Conveniance constant
        const size_t number_of_traces = m_TraceCount;
        const size_t number_of_samples = m_SampleCount;
        const size_t first_sample = m_SampleStart;
        const size_t last_sample = first_sample + m_SampleCount;

        std::vector<uint32_t> steps = (m_TraceStep > 0) ?
            numerics::ARange(m_TraceStep, m_TraceCount + 1, m_TraceStep) :
            std::vector<uint32_t>{ m_TraceCount };

        // Computing the expected output for each of the possible key using the model
        METRISCA_TRACE("Computing the expected output using the model (for each key bytes)");
        std::vector<Matrix<int32_t>> models; // for each KeyByte a matrix -> trace x 256
        models.reserve(m_Key.size());

        for (size_t i = 0; i != m_Key.size(); ++i) {
            // First set the corresponding byte index
            m_Distinguisher->GetPowerModel()->SetByteIndex(i);

            // Secondly compute the model        
            auto result = m_Distinguisher->GetPowerModel()->Model();
            if (result.IsError()) {
                METRISCA_ERROR("Fail to compute the model for KeyByte {}", i);
                return result.Error();
            }

            // Otherwise emplace back the result to the models
            models.emplace_back(std::move(result.Value()));
        }

        // Using our prior knowledge of the correct key, group each 
        // traces by the "expected" output.
        // Notice that in the scenario where we do not know the key, we can simply do this for each 
        // possible key hypothesis.
        METRISCA_TRACE("Group each traces by their \"expected\" output using the prior knowledge of the key");
        std::array<std::vector<size_t>, 256> grouped_by_expected_result; // Only store indices of the traces (to save memory)

        for (size_t key_byte = 0; key_byte != m_Key.size(); key_byte++) {
            for (size_t i = 0; i != number_of_traces; ++i) {
                int32_t expected_output = models[key_byte](i, m_Key[key_byte]);

                if (expected_output < 0 || expected_output >= 256) {
                    METRISCA_ERROR("Currently only model producing byte (in range 0 .. 255) are supported by this metric. Instead got {}", expected_output);
                    return Error::UNSUPPORTED_OPERATION;
                }

                grouped_by_expected_result[expected_output].push_back(i);
            }
        }
        
        // For each group, and for each step, compute the average within the group/step
        std::vector<std::array<std::vector<double>, 256>> group_average; // [step][expected result = 256][sample]
        group_average.reserve(steps.size());

        METRISCA_TRACE("Computing the average for each group");
        for (size_t stepIdx = 0; stepIdx != steps.size(); ++stepIdx) { 
            const size_t number_of_traces = steps[stepIdx]; // Shadow the global number_of_traces

            for (size_t groupIdx = 0; groupIdx != 256; ++groupIdx) { // For each of the possible output
                
                group_average[stepIdx][groupIdx].resize(number_of_samples, 0.0);
                size_t matching_trace_count = 0;

                for (size_t traceIdx : grouped_by_expected_result[groupIdx]) {
                    // Ignore all traces outside of the sweet zone
                    if (traceIdx >= number_of_traces) continue;
                    matching_trace_count += 1;

                    // Computing the average for this specific group
                    for (size_t sampleIdx = first_sample; sampleIdx != last_sample; sampleIdx++) {
                        group_average[stepIdx][groupIdx][sampleIdx - first_sample] += m_Dataset->GetSample(sampleIdx)[traceIdx];
                    }
                }

                // If no such traces exists
                if (matching_trace_count == 0) {
                    for (size_t j = 0; j < number_of_samples; j++) {
                        group_average[stepIdx][groupIdx][j] = DOUBLE_NAN;
                    }
                }
                else {
                    for (size_t j = 0; j < number_of_samples; j++) {
                        group_average[stepIdx][groupIdx][j] /= matching_trace_count;  
                    }
                }
            }
        }

        // For each group/step compute the covariance matrix between each sample
        std::vector<Matrix<double>> covariance_matrices;
        std::vector<Matrix<double>> inverse_covariance_matrices;
        covariance_matrices.resize(steps.size());

        METRISCA_TRACE("Computing the covariance matrices for each step, for each group between samples");
        for (size_t stepIdx = 0; stepIdx != steps.size(); stepIdx++) {
            const size_t number_of_traces = steps[stepIdx]; // Shadow the global number_of_traces

            Matrix<double>& M = covariance_matrices[stepIdx];
            M.Resize(number_of_samples, number_of_samples);

            // Reset the matrix to '0'
            for (size_t i = 0; i != number_of_samples; ++i) {
                M.FillRow(i, 0.0);
            }

            for (size_t groupIdx = 0; groupIdx != 256; ++groupIdx)
            {
                for (size_t traceIdx : grouped_by_expected_result[groupIdx]) {
                    if (traceIdx >= number_of_traces) continue; // Keep in mind we are working with a subset of all traces

                    for (size_t row = 0; row != number_of_samples; ++row) {
                        for (size_t col = 0; col != number_of_samples; ++col) {
                            M(row, col) +=
                                (m_Dataset->GetSample(first_sample + row)[traceIdx] - group_average[stepIdx][groupIdx][row]) *
                                (m_Dataset->GetSample(first_sample + col)[traceIdx] - group_average[stepIdx][groupIdx][col]);
                        }
                    }
                }
            }

            // Finally divide by the total number of traces
            for (size_t row = 0; row != number_of_samples; ++row) {
                for (size_t col = 0; col != number_of_samples; ++col) {
                    M(row, col) /= (number_of_traces - 1);
                }
            }
        }
        
        // Retrieve the key "probability" for each step
        std::vector<std::vector<std::array<double, 256>>> keyProbabilities; // [step][byte][byteValue]
        


        // Return success of the operatrion
        return {};
    }

}
