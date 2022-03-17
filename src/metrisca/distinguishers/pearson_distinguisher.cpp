/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/distinguishers/pearson_distinguisher.hpp"

#include "metrisca/utils/numerics.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/models/model.hpp"

namespace metrisca {

    Result<std::vector<std::pair<uint32_t, Matrix<double>>>, Error> PearsonDistinguisher::Distinguish()
    {
        auto result = InitializeResultMatrices();

        auto model_or_error = m_PowerModel->Model();
        if(model_or_error.IsError())
            return model_or_error.Error();
        auto model = model_or_error.Value();


        // Compute the pearson correlation coefficient between the model and the sample.
        // This operation is performed on every sample in the range.
        for(uint32_t s = m_SampleStart; s < m_SampleStart + m_SampleCount; s++)
        {
            nonstd::span<const int32_t> sample = m_Dataset->GetSample(s);
            for(uint32_t k = 0; k < 256; k++) 
            {
                std::array<double, 5> sums = std::array<double, 5>();
                uint32_t trace_count_start = 0;
                for(uint32_t step = 0; step < result.size(); step++)
                {
                    uint32_t step_trace_count = result[step].first;
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
                    
                    result[step].second(k, s - m_SampleStart) = std::abs(pearson_coeff);
                    trace_count_start = step_trace_count;
                }
            }
        }

        return result;
    }

}
