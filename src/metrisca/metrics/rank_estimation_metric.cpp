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

        // Finally return success of the initialization
        return {};
    }

    Result<void, Error> RankEstimationMetric::Compute() 
    {
        // Define a lazy function for the erf
        LazyFunction<double, double> lazy_normal_cdf = [](double x) -> double {
            return 0.5 + std::erf(x) / 2.0;
        };

        // Open the csv file and write csv header
        CSVWriter writer(m_OutputFile);
        writer << "Keybyte" << "Step";
        for (size_t i = 0; i < 256; i++) {
            writer << "value_" + std::to_string(i);
        }
        writer << csv::EndRow;

        // A temporary matrix to store the update sample (sample - expected value)
        std::vector<double> temp_samples(m_Dataset->GetHeader().NumberOfTraces);
        const size_t keyByteCount = m_Dataset->GetHeader().KeySize;

        // Resulting list of matrix
        // one LogProbabilityEntry per step
        struct LogProbabilityEntry {
            Matrix<double> data; // each matrix is (keyByteCount x 256)
            double min, max; // min and max value in the matrix 
            
            inline LogProbabilityEntry(size_t width, size_t height) 
            : data(width, height),
              min(std::numeric_limits<double>::infinity()),
              max(-std::numeric_limits<double>::infinity())
            { }
        };
        std::vector<LogProbabilityEntry> log_probabilities; 
        
        // Computing the resulting matrices
        METRISCA_INFO("Computing log probabilities for each byte in the key ({} bytes)", keyByteCount);
        for (size_t keyByteIdx = 0; keyByteIdx != keyByteCount; ++keyByteIdx)
        {
            // Loggingwriter
            METRISCA_INFO("Computing key byte {}/{}", keyByteIdx + 1, m_Dataset->GetHeader().PlaintextSize);

            // Retrieve the power model (and evaluate it)
            METRISCA_INFO("Compute power model for current metric");
            m_Distinguisher->GetPowerModel()->SetByteIndex(keyByteIdx);
            auto power_model_or_error = m_Distinguisher->GetPowerModel()->Model();
            if (power_model_or_error.IsError())
                return power_model_or_error.Error();

            auto power_model = power_model_or_error.Value();

            // Retrieve the pearson correlation
            METRISCA_INFO("Compute pearson correlation for the current metric");
            auto score_or_error = m_Distinguisher->Distinguish();
            if (score_or_error.IsError())
                return score_or_error.Error();

            auto score = score_or_error.Value();

            // If this is first key byte, initialize the entire log_probabilities resulting array (and allocate it)
            if (keyByteIdx == 0) {
                METRISCA_INFO("Initialize the log probabilities resulting array of Matrices");
                
                for (size_t i = 0; i < score.size(); i++) {
                    log_probabilities.emplace_back(256, keyByteCount);
                }
            }

            // Display progress bar
            indicators::BlockProgressBar lp_progress_bar{
                indicators::option::BarWidth(80),
                indicators::option::Start{"["},
                indicators::option::End{"]"},
                indicators::option::ShowElapsedTime{ true },
                indicators::option::ShowRemainingTime{ true },
                indicators::option::MaxProgress{ score.size() * 256 }
            };

            // Find the sample that leaks the most (highest pearson correlation with the model
            METRISCA_INFO("Compute log-likelihood under key hypothesis");
            for (size_t step = 0; step < score.size(); ++step)
            {
                // Find the number of trace in current batch
                const uint32_t step_trace_count = score[step].first;

                // Find the score (pearson correlation) for the current step
                const Matrix<double>& scores = score[step].second;

                // Find the corresponding entry in the log_probabilities
                auto& lpEntry = log_probabilities[step];

                // Start writing the new row
                writer << keyByteIdx << step;

                // For each key hypothesis find the sample with maximum correlation
                for (size_t key = 0; key != 256; ++key) {
                    // Step the progress bar
                    lp_progress_bar.tick();

                    // Find the sample with best correlation under given key hypothesis
                    size_t best_correlation_sample = numerics::ArgMax(scores.GetRow(key));

                    // Retrieve the corresponding sample and recenter it based on our 
                    // hypothesis
                    auto samples = m_Dataset->GetSample(best_correlation_sample).subspan(m_Distinguisher->GetSampleStart(), step_trace_count);
                    for (size_t i = 0; i != step_trace_count; ++i) {
                        temp_samples[i] = samples[i] - (double) power_model(key, m_Distinguisher->GetSampleStart() + i);
                    }

                    double standard_deviation = numerics::Std(temp_samples.data(), step_trace_count, 0.0);

                    // Pr[k_i | { X_q }] = Pr[{ X_q } | k_i] * Pr[k_i] / Sum{ Pr[{X_q} | k_i] * Pr[k_i] }
                    double log_proba_under_key_hypothesis = 1.0; // SUM log(Pr[ {X_q} | k_i ])
                    double cummulated_probability = 0.0; // SUM Pr[ {X_q} | k_i ]
                    for (size_t sampleIdx = 0; sampleIdx != samples.size(); ++sampleIdx) {
                        // Retrieve sample
                        double sample = temp_samples[sampleIdx];
                        double real_sample = samples[m_Distinguisher->GetSampleStart() + sampleIdx];

                        // Compute the probability of the current sample being measure at the current bin
                        double previous_bin_threshold = (sample - 0.5) / (METRISCA_SQRT_2 * standard_deviation);
                        double next_bin_threshold = (sample + 0.5) / (METRISCA_SQRT_2 * standard_deviation);

                        if (real_sample <= 1e-3) {
                            previous_bin_threshold = -std::numeric_limits<double>::infinity();
                        }
                        if (real_sample >= 255 - 1e-3) {
                            next_bin_threshold = std::numeric_limits<double>::infinity();
                        }

                        double partial_p = lazy_normal_cdf(next_bin_threshold) - lazy_normal_cdf(previous_bin_threshold);

                        log_proba_under_key_hypothesis += std::log(partial_p);
                        cummulated_probability += partial_p;
                    }

                    // Finally compute the log probability
                    double lp = log_proba_under_key_hypothesis - std::log(cummulated_probability);
                    lpEntry.data(keyByteIdx, key) = lp;
                    lpEntry.min = std::min(lp, lpEntry.min);
                    lpEntry.max = std::max(lp, lpEntry.max);
                    writer << lp;
                }

                writer << csv::EndRow;
            }

            // Terminate the lp progress bar
            lp_progress_bar.mark_as_completed();
        }

        // Compute the overall histogram as well as the bound of the error introducing when quantizing
        // the value into discrete bins
        METRISCA_INFO("Computing histogram with {} bins (for {} steps)", m_BinCount, log_probabilities.size());
        std::vector<std::vector<uint32_t>> histograms; // All histograms per steps
        writer << "Histograms" << csv::EndRow;

        for (size_t step = 0; step != log_probabilities.size(); ++step)
        {
            auto& lpEntry = log_probabilities[step];
            Matrix<uint32_t> histogram(m_BinCount, keyByteCount);

            // First compute the histograms
            for (size_t keyByte = 0; keyByte != keyByteCount; ++keyByte) {
                histogram.FillRow(keyByte, 0);

                for (size_t key = 0; key != 256; ++key) {
                    double value = lpEntry.data(keyByte, key);
                    size_t bin = numerics::FindBin(value, lpEntry.min, lpEntry.max, m_BinCount);
                    histogram(keyByte, bin) += 1;
                }
            }

            // Secondly perform keyByteCount convolution the retrieve the last sample
            METRISCA_INFO("Performing {} convolution to comupte the overall rank", keyByteCount - 1);
            auto first_row = histogram.GetRow(0);
            std::vector<uint32_t> conv(first_row.begin(), first_row.end());
            for (size_t i = 1; i < keyByteCount; ++i) {
                conv = numerics::Convolve<uint32_t>(nonstd::span<uint32_t>(conv.begin(), conv.end()), histogram.GetRow(i));
            }

            for (size_t i = 0; i < conv.size(); i++) {
                writer << conv[i];
            }
            writer << csv::EndRow;
            
            // Finally add the final histogram to the list
            histograms.push_back(std::move(conv));
        }
        
        // Print information to the screen
        METRISCA_TRACE("LazyErf cache efficiency (miss-rate): {}", lazy_normal_cdf.missRate());
        
        // Return success of the operatrion
        return {};
    }

}
