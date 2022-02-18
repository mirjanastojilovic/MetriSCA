/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "pearson_distinguisher.hpp"

#include "core/errors.hpp"
#include "core/trace_dataset.hpp"
#include "core/matrix.hpp"
#include "utils/numerics.hpp"

#include <cstdint>

namespace metrisca {

    int distinguishers::PearsonDistinguisher(std::vector<std::pair<uint32_t, Matrix<double>>>& out, 
                            const TraceDataset& dataset, 
                            const Matrix<int>& model, 
                            uint32_t sample_start, 
                            uint32_t sample_end, 
                            uint32_t trace_count, 
                            uint32_t trace_step)
    {
        uint32_t sample_count = sample_end - sample_start;
        if(trace_step > 0) 
        {
            // If the increment is greater than zero, generate a list of trace counts to individually compare
            std::vector<uint32_t> trace_counts = numerics::ARange(trace_step, trace_count+1, trace_step);
            out.resize(trace_counts.size());
            for(size_t i = 0; i < trace_counts.size(); ++i)
            {
                out[i].first = trace_counts[i];
                out[i].second = Matrix<double>(sample_count, 256);
            }
        }
        else
        {
            // If the increment is zero we simply use all the traces for the comparison
            out.resize(1);
            out[0].first = trace_count;
            out[0].second = Matrix<double>(sample_count, 256);
        }

        // Compute the pearson correlation coefficient between the model and the sample.
        // This operation is performed on every sample in the range.
        for(uint32_t s = sample_start; s < sample_end; s++)
        {
            nonstd::span<const int32_t> sample = dataset.GetSample(s);
            for(uint32_t k = 0; k < 256; k++) 
            {
                std::array<double, 5> sums = std::array<double, 5>();
                uint32_t trace_count_start = 0;
                for(uint32_t step = 0; step < out.size(); step++)
                {
                    uint32_t step_trace_count = out[step].first;
                    for(uint32_t t = trace_count_start; t < step_trace_count; t++)
                    {
                        const int32_t ts = sample[t];
                        const int32_t m = model(k, t);

                        sums[0] += m * ts;
                        sums[1] += m;
                        sums[2] += ts;
                        sums[3] += m * m;
                        sums[4] += ts * ts;
                    }

                    double divisor = std::sqrt(step_trace_count * sums[4] - sums[2] * sums[2]) * std::sqrt(step_trace_count * sums[3] - sums[1] * sums[1]);
                    double pearson_coeff = (step_trace_count * sums[0] - sums[1] * sums[2]) / divisor;
                    
                    out[step].second(k, s - sample_start) = std::abs(pearson_coeff);
                    trace_count_start = step_trace_count;
                }
            }
        }

        return SCA_OK;
    }

}