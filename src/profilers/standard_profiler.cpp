/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "profilers/standard_profiler.hpp"

#include "core/errors.hpp"
#include "core/matrix.hpp"
#include "core/trace_dataset.hpp"
#include "utils/crypto.hpp"
#include "utils/numerics.hpp"

#include <vector>
#include <array>
#include <nonstd/span.hpp>

namespace metrisca {

    static void ProfilerSBOXFixed(Matrix<double>& out, const TraceDataset& dataset, unsigned char key, unsigned int byte_index)
    {
        TraceDatasetHeader header = dataset.GetHeader();
        uint32_t trace_count = header.NumberOfTraces;
        uint32_t sample_count = header.NumberOfSamples;

        // Compute output of the SBOX with the specified key
        // NOTE: this assumes the identity model, which could maybe not be
        // the best in some cases ?
        std::vector<uint8_t> y;
        y.reserve(trace_count);
        for(uint32_t t = 0; t < trace_count; t++)
        {
            auto& plaintext = dataset.GetPlaintext(t);
            const uint8_t pt = plaintext[byte_index];
            y.push_back(crypto::SBox(pt ^ key));
        }

        // Compute the correlation between y and every sample
        std::vector<double> corr;
        corr.reserve(sample_count);
        for(uint32_t s = 0; s < sample_count; s++)
        {
            auto& sample = dataset.GetSample(s);
            double pearson = numerics::PearsonCorrelation(y, sample);
            corr.push_back(std::abs(pearson));
        }

        // The best sample is the one with the highest pearson correlation
        size_t best_sample_index = numerics::ArgMax(corr);

        // Contains the trace values for different key classes
        std::array<std::vector<int32_t>, 256> classes;
        auto& best_sample = dataset.GetSample((uint32_t)best_sample_index);
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

    static int ProfileSBOX(Matrix<double>& out, const TraceDataset& dataset, unsigned char key, unsigned int byte_index)
    {
        TraceDatasetHeader header = dataset.GetHeader();

        // NOTE: Only fixed key is currently supported
        switch(header.KeyMode)
        {
        case KeyGenerationMode::FIXED: {
            ProfilerSBOXFixed(out, dataset, key, byte_index);
        } break;
        default: return SCA_UNSUPPORTED_OPERATION;
        }

        return SCA_OK;
    }

    int profilers::StandardProfiler(Matrix<double>& out, const TraceDataset& dataset, unsigned char key, unsigned int byte_index)
    {
        out = Matrix<double>(256, 2); // First row is the mean and the second row is the standard deviation for each key

        // NOTE: Only S-BOX is currently supported
        TraceDatasetHeader header = dataset.GetHeader();
        switch(header.EncryptionType)
        {
        case EncryptionAlgorithm::S_BOX: {
            int result = ProfileSBOX(out, dataset, key, byte_index);
        } break;
        default: return SCA_UNSUPPORTED_OPERATION;
        }

        return SCA_OK;
    }

}