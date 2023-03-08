/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/key_enumeration_metric.hpp"

#include "metrisca/core/lazy_function.hpp"
#include "metrisca/core/errors.hpp"
#include "metrisca/core/logger.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/core/indicators.hpp"
#include "metrisca/core/plugin.hpp"
#include "metrisca/models/model.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/utils/crypto.hpp"
#include "metrisca/profilers/profiler.hpp"

#define DEFAULT_ENUMERATED_KEY_COUNT 4096

namespace metrisca {

    Result<void, Error> KeyEnumerationMetric::Init(const ArgumentList& args)
    {
        // Retrieve the number of key to enumerate
        m_KeyCount = args.GetUInt32(ARG_NAME_ENUMERATED_KEY_COUNT)
                         .value_or(DEFAULT_ENUMERATED_KEY_COUNT);

        // Retrieve the dataset and the traceCount / traceStep
        auto dataset_arg = args.GetDataset(ARG_NAME_DATASET);
        auto trace_count_arg = args.GetUInt32(ARG_NAME_TRACE_COUNT);
        auto trace_step_arg = args.GetUInt32(ARG_NAME_TRACE_STEP);
        auto byte_idx_arg = args.GetUInt32(ARG_NAME_BYTE_INDEX);
        auto bin_size_arg = args.GetUInt32(ARG_NAME_BIN_SIZE);

        if (!dataset_arg.has_value())
        {
            METRISCA_ERROR("KeyEnumerationMetric require argument {} to be specified but was not provided",
                           ARG_NAME_DATASET);
            return Error::MISSING_ARGUMENT;
        }

        m_Dataset = dataset_arg.value();
        auto dataset_header = m_Dataset->GetHeader();

        m_TraceCount = trace_count_arg.value_or(dataset_header.NumberOfTraces);
        m_TraceStep = trace_step_arg.value_or(0);
        m_ByteIndex = byte_idx_arg.value_or(0);
        m_BinSize = bin_size_arg.value_or(1);

        if (m_BinSize <= 0)
        {
            METRISCA_ERROR("BinSize cannot be of null size");
            return Error::INVALID_ARGUMENT;
        }

        if (m_Dataset->GetHeader().KeyMode != KeyGenerationMode::FIXED)
        {
            METRISCA_ERROR("Cannot perform key enumeration on non-fixed key");
            return Error::UNSUPPORTED_OPERATION;
        }

        if (m_TraceCount > dataset_header.NumberOfTraces)
        {
            METRISCA_ERROR("User-specified number of traces ({}) is greater than the number of traces within the database ({})",
                           m_TraceCount,
                           dataset_header.NumberOfTraces);
            return Error::INVALID_ARGUMENT;
        }

        if (m_TraceCount <= 0)
        {
            METRISCA_ERROR("KeyEnumerationMetric require at least one trace");
            return Error::INVALID_ARGUMENT;
        }

        if (m_TraceStep >= m_TraceCount)
        {
            METRISCA_ERROR("User-specified number of step exceed the configured number of traces");
            return Error::INVALID_ARGUMENT;
        }

        // Retrieve each of the subkey configuration
        auto subkey_list_arg = args.GetSubList(ARG_NAME_SUBKEY);
        if (!subkey_list_arg.has_value() || subkey_list_arg.value().empty())
        {
            METRISCA_ERROR("Require at least one subkey descriptor in order to run the key enumeration metric");
            return Error::INVALID_ARGUMENT;
        }

        auto subkey_list = subkey_list_arg.value();
        for (size_t i = 0; i != subkey_list.size(); ++i)
        {
            auto result = parseSubkey(subkey_list[i], i);
            if (result.IsError())
            {
                return result.Error();
            }
        }

        // Ensure each subkey descriptors has been corretly initialized
        for (size_t i = 0; i < m_KeyByteDescriptors.size(); i++)
        {
            if (!m_KeyByteDescriptors[i].IsInitialized())
            {
                METRISCA_ERROR("No key descriptor for byte {}. Aborting.", i);
                return Error::MISSING_ARGUMENT;
            }
        }

        // Logging duty
        METRISCA_INFO("KeyEnumerationMetric configured to enumerate at most {} keys on at most {} traces in {} step",
                      m_KeyCount,
                      m_TraceCount,
                      m_TraceStep);

        // Return success of initialization operation
        return {};
    }

    Result<void, Error> KeyEnumerationMetric::parseSubkey(const ArgumentList& subkey, size_t idx)
    {
        // retrieve all arguments corresponding with current subkey
        auto byte_idx_arg = subkey.GetUInt32(ARG_NAME_BYTE_INDEX);
        auto model_arg = subkey.GetString(ARG_NAME_MODEL);
        auto sample_tuple_arg = subkey.GetTupleUInt32(ARG_NAME_SAMPLE_TUPLE);

        // require all argument to be present
        if (!byte_idx_arg.has_value() || !model_arg.has_value() || !sample_tuple_arg.has_value())
        {
            METRISCA_ERROR("Missing argument on subkey indexed {}", idx);
            return Error::MISSING_ARGUMENT;
        }

        // retrieve the value of each arguments
        uint32_t byte_idx = byte_idx_arg.value();
        std::string model = model_arg.value();
        auto [sampleStart, sampleEnd] = sample_tuple_arg.value();

        // Checks values
        if (sampleStart >= sampleEnd)
        {
            METRISCA_ERROR("KeyEnumerationMetric does not support 0-sample subkey. Invalid sample range [{}, {}) for subkey indexed {}", sampleStart, sampleEnd, idx);
            return Error::INVALID_ARGUMENT;
        }

        if (sampleEnd > m_Dataset->GetHeader().NumberOfSamples)
        {
            METRISCA_ERROR("Invalid sampleEnd for subkey indexed {}, (sample end is {} while there is only {} samples in dataset)", idx, sampleEnd, m_Dataset->GetHeader().NumberOfSamples);
            return Error::INVALID_ARGUMENT;
        }

        // Try instantiating the model corresponding with the model string
        ArgumentList powerModelArgument;
        powerModelArgument.SetUInt32(ARG_NAME_BYTE_INDEX, m_ByteIndex); // this is the index of the byte within the plaintext we are encoding/decoding
        powerModelArgument.SetDataset(ARG_NAME_DATASET, m_Dataset);

        auto modelResult = PluginFactory::The().ConstructAs<PowerModelPlugin>(PluginType::PowerModel, model, powerModelArgument);

        if (modelResult.IsError())
        {
            METRISCA_ERROR("Failed to instantiate the specified model \"{}\" for subkey indexed {}", model, idx);
            return modelResult.Error();
        }

        // Setup the subkey descriptor
        if (byte_idx >= m_KeyByteDescriptors.size())
        {
            m_KeyByteDescriptors.resize(byte_idx + 1, KeyByteDescriptor{});
        }
    
        // Ensure this specific by is not yet setup
        if (m_KeyByteDescriptors[byte_idx].IsInitialized())
        {
            METRISCA_ERROR("Multiple descriptor for the subkey byte {} is not yet supported by this metrisca", byte_idx);
            return Error::UNSUPPORTED_OPERATION;
        }

        // Lastly set the descriptor vaue
        m_KeyByteDescriptors[byte_idx].Model = std::move(modelResult.Value());
        m_KeyByteDescriptors[byte_idx].SampleStart = sampleStart;
        m_KeyByteDescriptors[byte_idx].SampleEnd = sampleEnd;

        // Logging duty
        METRISCA_INFO("subkey for byte {} setup with model \"{}\" on sample {}:{}",
            byte_idx,
            model,
            sampleStart,
            sampleEnd);

        // Return success
        return {};
    }

    Result<void, Error> KeyEnumerationMetric::Compute()
    {
        LazyFunction<double, double> lazyErf = [](double x) -> double {
            return std::erf(x);
        };
        std::vector<uint32_t> trace_counts = { m_TraceCount };
        if (m_TraceStep > 0)
        {
            trace_counts = numerics::ARange(m_TraceStep, m_TraceCount + 1, m_TraceStep);
        }

        // Generate the model for each byte for each key
        // TODO: make so that the model don't need to be evaluated on each trace when fixed key
        std::vector<Matrix<int>> modelledLeakageList;
        modelledLeakageList.reserve(m_KeyByteDescriptors.size());

        {
            HideCursorGuard guard;
            indicators::BlockProgressBar bar{
                indicators::option::BarWidth(60),
                indicators::option::MaxProgress(m_KeyByteDescriptors.size()),
                indicators::option::ShowElapsedTime(true),
                indicators::option::ShowRemainingTime(true),
            };

            for (uint32_t keyByte = 0; keyByte != m_KeyByteDescriptors.size(); ++keyByte)
            {
                bar.set_option(indicators::option::PostfixText{
                    "Modelling leakage " + std::to_string(keyByte + 1) + "/" + std::to_string(m_KeyByteDescriptors.size())
                });
                bar.tick();

                auto result = m_KeyByteDescriptors[keyByte].Model->Model();
                if (result.IsError()) {
                    bar.set_option(indicators::option::ForegroundColor{indicators::Color::red});
                    bar.mark_as_completed();
                    METRISCA_ERROR("Failed to model byte {}", keyByte);
                    return result.Error();
                }

                modelledLeakageList.emplace_back(result.Value());
            }

            bar.mark_as_completed();
        }

        //tex:
        // $$\mathbb{P}[\theta_i | l_i]$$
        // Compute the error for each trace
        Matrix<double> partialKeyProbabilityEstimator(256 * m_KeyByteDescriptors.size(), trace_counts.size()); 

        {
            HideCursorGuard guard;
            indicators::BlockProgressBar mainbar{
                indicators::option::BarWidth(60),
                indicators::option::MaxProgress(m_KeyByteDescriptors.size() * 256 * trace_counts.size()),
                indicators::option::ShowElapsedTime(true),
                indicators::option::ShowRemainingTime(true),
                indicators::option::ForegroundColor{ indicators::Color::white }
            };

            for (uint32_t trace_count_idx = 0, it = 0; trace_count_idx < trace_counts.size(); trace_count_idx++)
            {
                const uint32_t current_trace_count = trace_counts[trace_count_idx];

                for (uint32_t key_byte_idx = 0; key_byte_idx != m_KeyByteDescriptors.size(); ++key_byte_idx)
                {
                    auto& keyByteDescriptor = m_KeyByteDescriptors[key_byte_idx];
                    double renormalizationTerm = 0.0;

                    for (uint32_t key_idx = 0; key_idx != 256; key_idx++, it++)
                    {
                        // update the loading bar to give feedback to the awaiting user
                        mainbar.set_option(indicators::option::PostfixText{
                                "Model evaluation " + std::to_string(it + 1) + "/" + std::to_string(m_KeyByteDescriptors.size() * 256 * trace_counts.size()) 
                                    + " subkey " + std::to_string(key_byte_idx) + "/" + std::to_string(m_KeyByteDescriptors.size())
                            });
                        mainbar.tick();

                        // for each sample compute the hypothesis probability
                        //tex:
                        // $$P[\theta_k | \{ l_{0;i} \cdots l_{N;i} \}]$$
                        // where $i \in 0 \cdots M$ is the current trace index
                        for (size_t trace_idx = 0; trace_idx != current_trace_count; trace_idx++) {
                            // Compute the expected mean (under the assumption of a certain model)
                            // const int expectedMean = modelledLeakageList[key_byte_idx].GetRow(0)[key_idx]
                            const double expectedMean = (double) modelledLeakageList[key_byte_idx](trace_idx, key_idx);

                            // Compute the standard devation for the current subkey on the current trace
                            double stdDev = 0.0;
                            for (uint32_t sampleIdx = keyByteDescriptor.SampleStart; sampleIdx != keyByteDescriptor.SampleEnd; ++sampleIdx) {
                                double value = (double) m_Dataset->GetSample(sampleIdx)(trace_idx);
                                stdDev += (value - expectedMean) * (value - expectedMean);
                            }
                            stdDev = std::sqrt(stdDev);

                            // Compute the probability of the model under the assumption the noise is gaussian an null 
                        }

                    }

                    // for (uint32_t key_idx = 0; key_idx != 256; key_idx++, it++)
                    // {
                    //     mainbar.set_option(indicators::option::PostfixText{
                    //             "Model evaluation " + std::to_string(it + 1) + "/" + std::to_string(m_KeyByteDescriptors.size() * 256 * trace_counts.size()) 
                    //                 + " subkey " + std::to_string(key_byte_idx) + "/" + std::to_string(m_KeyByteDescriptors.size())
                    //         });
                    //     mainbar.tick();


                    //     // Compute the standard deviation for the current samples
                    //     const int expectedMean = modelledLeakageList[key_byte_idx].GetRow(0)[key_idx];
                    //     double stdDev = 0.0;
                    //     for (uint32_t sampleIdx = keyByteDescriptor.SampleStart; sampleIdx != keyByteDescriptor.SampleEnd; ++sampleIdx) {
                    //         nonstd::span_lite::span<const int> sample = m_Dataset->GetSample(sampleIdx).subspan(0, trace_counts[trace_count_idx]);
                    //         stdDev += numerics::Variance(sample.subspan(0, trace_counts[trace_count_idx]), (double)expectedMean);
                    //     }
                    //     stdDev = std::sqrt(stdDev / (keyByteDescriptor.SampleEnd - keyByteDescriptor.SampleStart));

                    //     // Compute the probability of the model under the assumption the noise is gaussian and null-mean
                    //     // Do not use logarithm !!
                    //     // https://cs.stackexchange.com/questions/91538/why-is-adding-log-probabilities-considered-numerically-stable
                    //     double probability = 1.0;
                    //     for (uint32_t sampleIdx = keyByteDescriptor.SampleStart; sampleIdx != keyByteDescriptor.SampleEnd; ++sampleIdx) {
                    //         nonstd::span_lite::span<const int> sample = m_Dataset->GetSample(sampleIdx).subspan(0, trace_counts[trace_count_idx]);

                    //         for (size_t idx = 0; idx != sample.size(); idx++) {
                    //             const double bin = (double)(sample[idx] - expectedMean) / ((double)m_BinSize);

                    //             double pAtLeastThisBin = 0.5 * lazyErf((bin - 0.5) / (stdDev * METRISCA_SQRT_2));
                    //             if (bin <= 0.5) {
                    //                 pAtLeastThisBin = 0.5 + pAtLeastThisBin;
                    //             }
                    //             else {
                    //                 pAtLeastThisBin = 0.5 - pAtLeastThisBin;
                    //             }

                    //             double pNotNextBinOrHigher = 0.5 * lazyErf((bin + 0.5) / (stdDev * METRISCA_SQRT_2));
                    //             if (bin <= -0.5) {
                    //                 pNotNextBinOrHigher = 0.5 + pNotNextBinOrHigher;
                    //             }
                    //             else {
                    //                 pNotNextBinOrHigher = 0.5 - pNotNextBinOrHigher;
                    //             }

                    //             //tex:
                    //             // $$\mathbb{P}[l_k \ | \ \theta_k]$$
                    //             // ASSUMPTION: $\forall i \neq j \in \mathbb{N}, \quad (l_k \ | \ \theta_k) \perp (l_j \ | \ \theta_k) $
                    //             probability *= (pAtLeastThisBin - pNotNextBinOrHigher);
                    //         }
                    //     }
                        
                    //     //tex:
                    //     // probability : $P[\{l_0 \cdots l_N\} \ | \ \theta_k] \ \cdot \ P[\theta_k] $
                    //     partialKeyProbabilityEstimator(trace_count_idx, key_byte_idx * 256 + key_idx) = probability / 256.0;
                    //     renormalizationTerm += probability;
                    // }

                    // for (uint32_t key_idx = 0; key_idx != 256; key_idx++) {
                    //     partialKeyProbabilityEstimator(trace_count_idx, key_byte_idx * 256 + key_idx) /= renormalizationTerm;
                    // }
                }
            }

            mainbar.mark_as_completed();
            METRISCA_TRACE("LazyFunction for `erf`, cache miss rate: {}", lazyErf.missRate());
        }


        // Trivial computation
        return {};
    }

}
