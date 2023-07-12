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
#include "metrisca/scores/score.hpp"

#include <limits>
#include <atomic>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <math.h>

#define DOUBLE_NAN (std::numeric_limits<double>::signaling_NaN())
#define DOUBLE_INFINITY (std::numeric_limits<double>::infinity())
#define IS_VALID_DOUBLE(value) (std::isfinite(value) && !std::isnan(value))

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

        // Retrieve the score plugin
        auto score_plugin_name = args.GetString(ARG_NAME_SCORES);
        if (!score_plugin_name.has_value()) {
            METRISCA_ERROR("Fail to retrieve the score plugin name");
            return Error::INVALID_ARGUMENT;
        }

        METRISCA_TRACE("Loading score plugin {}", score_plugin_name.value());
        auto score_plugin_or_error = PluginFactory::The().ConstructAs<ScorePlugin>(PluginType::Score, score_plugin_name.value(), args);
        if (score_plugin_or_error.IsError()) {
            METRISCA_ERROR("Fail to load the score plugin {}", score_plugin_name.value());
            return score_plugin_or_error.Error();
        }
        m_ScorePlugin = std::move(score_plugin_or_error.Value());


        return  {};
    }

    Result<void, Error> RankEstimationMetric::Compute() 
    {
        // Retrieve the probabilities for each step and each bytes
        std::vector<std::pair<uint32_t, std::vector<std::array<double, 256UL>>>> probabilitiesPerSteps;
        {
            auto score_or_error = m_ScorePlugin->ComputeScores();
            if (score_or_error.IsError()) {
                METRISCA_ERROR("Fail to compute the scores");
                return score_or_error.Error();
            }
            probabilitiesPerSteps = std::move(score_or_error.Value());
        }
        size_t const stepCount = probabilitiesPerSteps.size();

        // Write CSV 
        CSVWriter writer(m_OutputFile);
        writer << "number-of-traces" << "key-byte" << "keys..." << csv::EndRow;
        for (auto& [traceCount, probabilities] : probabilitiesPerSteps) {
            for (size_t byteIdx = 0; byteIdx != probabilities.size(); byteIdx++) {
                writer << traceCount << byteIdx;
                for (auto& value : probabilities[byteIdx]) {
                    writer << value;
                }
                writer << csv::EndRow;
            }
        }

        // Do some stuff with things that make more stuff togethers
        METRISCA_INFO("Computing histogram in order to approximate the rank of the whole key within our model with {} bins", m_BinCount);
        std::vector<std::vector<uint32_t>> histograms; // A list of all of the output histograms
        std::vector<std::pair<double, double>> minMaxValues; // A list of min/max for each matrix
        std::vector<uint64_t> totalCounts; // Sum over the histogram

        // Because the following code is executed in parallel we must ensure the container won't be resized during
        // execution
        histograms.resize(probabilitiesPerSteps.size(), {}); 
        minMaxValues.resize(probabilitiesPerSteps.size(), std::make_pair(0.0, 0.0)); 
        totalCounts.resize(probabilitiesPerSteps.size(), 0);

        metrisca::parallel_for("Aggregating histograms together", 0, probabilitiesPerSteps.size(), [&](size_t stepIdx) {
            const auto& log_probabilities = probabilitiesPerSteps[stepIdx].second;

            // Find the minimum/maximum of each log_probabilities (non standard value are skipped)
            double min = DOUBLE_INFINITY, max = -DOUBLE_INFINITY;

            for (size_t keyByte = 0; keyByte != stepCount; ++keyByte) {
                for (size_t key = 0; key != 256; ++key) {
                    double value = log_probabilities[keyByte][key];

                    if (IS_VALID_DOUBLE(value)) {
                        max = std::max(max, value);
                        min = std::min(min, value);
                    }
                }
            }

            minMaxValues[stepIdx] = std::make_pair(min, max);

            // Compute the histogram 
            Matrix<uint32_t> histogram(m_BinCount, m_Key.size());
            for (size_t keyByte = 0; keyByte != m_Key.size(); ++keyByte) {
                histogram.FillRow(keyByte, 0);

                for (size_t key = 0; key != 256; ++key) {
                    // Retrieve the value corresponding to the current key-byte & key
                    double value = log_probabilities[keyByte][key];

                    // Skip invalid probabilities entry sample
                    if (!IS_VALID_DOUBLE(value))
                        continue;
                    
                    // Find corresponding bin within histogram and increment it
                    size_t bin = numerics::FindBin(value, min, max, m_BinCount);
                    histogram(keyByte, bin) += 1;
                }
            }

            // Compute the convolution between each sample
            auto first_row = histogram.GetRow(0);
            std::vector<uint32_t> conv(first_row.begin(), first_row.end()); // So un-optimized that it make me sick (hard copy)
            for (size_t i = 1; i < m_Key.size(); ++i) {
                conv = numerics::Convolve<uint32_t>(nonstd::span<uint32_t>(conv.begin(), conv.end()), histogram.GetRow(i));
            }

            // Iterate through the histogram
            uint64_t count = 0;
            for (uint32_t v : conv) {
                count += v;
            }
            
            // Finally add the last histogram to the output list
            histograms[stepIdx] = std::move(conv);
            totalCounts[stepIdx] = count;
        });

        // Dump the output histogram to the file
        METRISCA_INFO("Writing histogram to the file");
        writer << "number-of-traces" << "histograms..." << csv::EndRow;

        for (size_t stepIdx = 0; stepIdx != stepCount; ++stepIdx) {
            writer << probabilitiesPerSteps[stepIdx].first;

            // Print every entry within histograms
            for (uint32_t entry : histograms[stepIdx]) {
                writer << entry;
            }
            writer << csv::EndRow;
        } 
        writer << csv::Flush;

        // Computing key rank for each histograms
        writer << "number-of-traces" << "lower_bound" << "upper_bound" << "rank" << "histogram-entry" << csv::EndRow;

        METRISCA_INFO("Computing and bounding key-rank of the real key");
        for (size_t stepIdx = 0; stepIdx != stepCount; ++stepIdx) {
            const auto& histogram = histograms[stepIdx];
            const auto& entry = probabilitiesPerSteps[stepIdx].second;
            auto [min, max] = minMaxValues[stepIdx];

            // Determine the bin in which the real key should be in theory
            double log_probability_correct_key = 0.0;
            for (size_t keyByte = 0; keyByte != m_Key.size(); ++keyByte) {
                double increment = entry[keyByte][m_Key[keyByte]];

                if (IS_VALID_DOUBLE(increment))
                    log_probability_correct_key += increment;
                else {
                    METRISCA_WARN("The log_probability for byte {} and value {} (with {} traces) is not defined", keyByte, m_Key[keyByte], probabilitiesPerSteps[stepIdx].first);

                    // In this scenario we cannot infer the given key byte, as such we can take the worst value possible
                    log_probability_correct_key += min;
                }
            }

            // Find the corresponding bin
            size_t correct_key_bin = numerics::FindBin(log_probability_correct_key, min * m_Key.size(), max * m_Key.size(), histogram.size());

            // Compute the overall key rank (and bounds the bin quantization error) using the histogram
            size_t lower_bound = 0;
            size_t upper_bound = 0;
            size_t rank = 0;

            for(size_t i = correct_key_bin + m_Key.size(); i < histogram.size(); i++) {
                lower_bound += histogram[i];
            }

            for (size_t i = correct_key_bin; i < correct_key_bin + m_Key.size() && i < histogram.size(); i++) {
                rank += histogram[i];
            }

            for(size_t i = ((correct_key_bin < m_Key.size()) ? 0 : (correct_key_bin - m_Key.size())); i < correct_key_bin && i < histogram.size(); i++) {
                upper_bound += histogram[i];
            }
            
            rank += lower_bound;
            upper_bound += rank;

            // Output these values to the file
            writer << probabilitiesPerSteps[stepIdx].first << lower_bound << upper_bound << rank << totalCounts[stepIdx] << csv::EndRow;
        }

        // Return success of the operatrion
        return {};
    }

}
