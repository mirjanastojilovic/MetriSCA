/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/profilers/standard_profiler.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/matrix.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/utils/crypto.hpp"
#include "metrisca/utils/numerics.hpp"

#include <vector>
#include <array>

namespace metrisca {

    void StandardProfiler::ProfilerSBOXFixed(Matrix<double>& out) const
    {
        TraceDatasetHeader header = m_Dataset->GetHeader();
        uint32_t trace_count = header.NumberOfTraces;
        uint32_t sample_count = header.NumberOfSamples;

        // Compute output of SBOX with the specified key
        // NOTE: this assumes the identity model, which could maybe not be
        // the best in some cases ?
        std::vector<uint8_t> y;
        y.reserve(trace_count);
        for(uint32_t t = 0; t < trace_count; t++)
        {
            auto& plaintext = m_Dataset->GetPlaintext(t);
            const uint8_t pt = plaintext[m_ByteIndex];
            y.push_back(crypto::SBox(pt ^ m_KnownKey));
        }

        // Compute the correlation between y and every sample
        std::vector<double> corr;
        corr.reserve(sample_count);
        for(uint32_t s = 0; s < sample_count; s++)
        {
            auto& sample = m_Dataset->GetSample(s);
            double pearson = numerics::PearsonCorrelation(y, sample);
            corr.push_back(std::abs(pearson));
        }

        // The best sample is the one with the highest pearson correlation
        size_t best_sample_index = numerics::ArgMax(corr);

        // Contains the trace values for different key classes
        std::array<std::vector<int32_t>, 256> classes;
        auto& best_sample = m_Dataset->GetSample((uint32_t)best_sample_index);
        for(uint32_t t = 0; t < trace_count; t++)
        {
            uint8_t trace_y = y[t];
            classes[trace_y].push_back(best_sample[t]);
        }

        // Compute the profile for each key
        for(uint32_t k = 0; k < 256; k++)
        {
            double mean = numerics::Mean(classes[k]);
            double std = numerics::Std(classes[k], mean);
            out(0, k) = mean;
            out(1, k) = std;
        }
    }

    Result<void, int> StandardProfiler::ProfileSBOX(Matrix<double>& out) const
    {
        TraceDatasetHeader header = m_Dataset->GetHeader();

        // NOTE: Only fixed key is currently supported
        switch(header.KeyMode)
        {
        case KeyGenerationMode::FIXED: {
            ProfilerSBOXFixed(out);
        } break;
        default: return SCA_UNSUPPORTED_OPERATION;
        }

        return {};
    }

    Result<Matrix<double>, int> StandardProfiler::Profile()
    {
        Matrix<double> result(256, 2); // First row is the mean and the second row is the standard deviation for each key

        // NOTE: Only S-BOX is currently supported
        TraceDatasetHeader header = m_Dataset->GetHeader();
        switch(header.EncryptionType)
        {
        case EncryptionAlgorithm::S_BOX: {
            auto profiler_result = ProfileSBOX(result);
            if(profiler_result.IsError())
                return profiler_result.Error();
        } break;
        default: return SCA_UNSUPPORTED_OPERATION;
        }

        return result;
    }

}