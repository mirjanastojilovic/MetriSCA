/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/pi_metric.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/profilers/profiler.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/utils/crypto.hpp"

#include <array>
#include <vector>

namespace metrisca {

    Result<void, int> PIMetric::Init(const ArgumentList& args)
    {
        auto base_result = MetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result.Error();

        auto training = args.GetDataset(ARG_NAME_TRAINING_DATASET);
        auto testing = args.GetDataset(ARG_NAME_TESTING_DATASET);
        auto profiler = args.GetString(ARG_NAME_PROFILER);
        auto byte_index = args.GetUInt32(ARG_NAME_BYTE_INDEX);
        auto known_key = args.GetUInt8(ARG_NAME_KNOWN_KEY);
        auto sigma = args.GetDouble(ARG_NAME_SIGMA);

        if(!training.has_value())
            return SCA_MISSING_ARGUMENT;

        if(!testing.has_value())
            return SCA_MISSING_ARGUMENT;

        if(!profiler.has_value())
            return SCA_MISSING_ARGUMENT;

        if(!known_key.has_value())
            return SCA_MISSING_ARGUMENT;

        m_TrainingDataset = training.value();
        m_TestingDataset = testing.value();
        m_ByteIndex = byte_index.value_or(0);
        m_KnownKey = known_key.value();
        m_Sigma = sigma.value_or(0.0);

        auto profiler_or_error = PluginFactory::The().ConstructAs<ProfilerPlugin>(PluginType::Profiler, profiler.value(), args);
        if(profiler_or_error.IsError())
            return profiler_or_error.Error();

        m_Profiler = profiler_or_error.Value();

        auto training_header = m_TrainingDataset->GetHeader();
        auto testing_header = m_TestingDataset->GetHeader();

        if(training_header.KeySize != testing_header.KeySize)
            return SCA_INVALID_ARGUMENT;

        if(training_header.PlaintextSize != testing_header.PlaintextSize)
            return SCA_INVALID_ARGUMENT;

        if(m_ByteIndex >= testing_header.PlaintextSize)
            return SCA_INVALID_ARGUMENT;

        return {};
    }

    Result<void, int> PIMetric::Compute()
    {
        // Find the best testing sample using CPA
        uint32_t testing_trace_count = m_TestingDataset->GetHeader().NumberOfTraces;
        uint32_t testing_sample_count = m_TestingDataset->GetHeader().NumberOfSamples;

        // Compute the identity model for each plaintext and the selected key
        std::vector<uint8_t> y;
        y.reserve(testing_trace_count);
        for(uint32_t t = 0; t < testing_trace_count; t++)
        {
            auto& plaintext = m_TestingDataset->GetPlaintext(t);
            const uint8_t pt = plaintext[m_ByteIndex];
            y.push_back(crypto::SBox(pt ^ m_KnownKey));
        }

        // Compute the correlation between the model and every sample
        std::vector<double> corr;
        corr.reserve(testing_sample_count);
        for(uint32_t s = 0; s < testing_sample_count; s++)
        {
            auto& sample = m_TestingDataset->GetSample(s);
            double pearson = numerics::PearsonCorrelation(y, sample);
            corr.push_back(std::abs(pearson));
        }

        // Best sample is the one with the highest correlation
        size_t best_testing_sample_index = numerics::ArgMax(corr);

        // Partition the traces of the best sample into classes based on the model output
        std::array<std::vector<int32_t>, 256> testing_classes;
        auto& best_testing_sample = m_TestingDataset->GetSample((uint32_t)best_testing_sample_index);
        for(uint32_t t = 0; t < testing_trace_count; t++)
        {
            const uint8_t trace_model = y[t];
            testing_classes[trace_model].push_back(best_testing_sample[t]);
        }

        auto profile_or_error = m_Profiler->Profile();
        if(profile_or_error.IsError())
            return profile_or_error.Error();

        auto profile = profile_or_error.Value();

        // If a sigma parameter is specified (greather than zero),
        // override the sigmas loaded from the profile
        if (m_Sigma > 0.0)
        {
            profile.FillRow(1, m_Sigma);
        }

        // Recover profile information from the training dataset
        const auto& means = profile.GetRow(0);
        const auto& stds = profile.GetRow(1);
        std::vector<double> invstds;
        invstds.reserve(stds.size());
        for(size_t i = 0; i < stds.size(); i++)
        {
            invstds.push_back(1.0 / stds[i]);
        }

        // Compute the perceived information
        double pi = 8.0;
        for(uint32_t k = 0; k < 256; k++)
        {
            auto& current_class = testing_classes[k];
            size_t class_size = current_class.size();
            double class_p = 0.0;
            for(size_t t = 0; t < class_size; t++)
            {
                int32_t trace = current_class[t];
                
                double sum_gaussian = 0.0;
                double current_key_gaussian = 0.0;
                for(uint32_t kc = 0; kc < 256; kc++)
                {
                    double g = math::Gaussian((double)trace, means[kc], invstds[kc]);
                    if(kc == k)
                    {
                        current_key_gaussian = g;
                    }
                    sum_gaussian += g;
                }
                class_p += std::log2(current_key_gaussian / sum_gaussian);
            }
            if(class_size > 0)
            {
                class_p /= (double)class_size * 256.0;
            }
            pi += class_p;
        }

        CSVWriter writer(m_OutputFile);
        writer << "pi" << "avg_sigma" << csv::EndRow;
        writer << pi << numerics::Mean(stds) << csv::EndRow;

        return {};
    }

}