/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */
#include "metrisca/distinguishers/pearson_distinguisher.hpp"

#include "metrisca/core/logger.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/core/indicators.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/models/model.hpp"

namespace metrisca {

    Result<void, Error> PearsonDistinguisher::Init(const ArgumentList& args)
    {
        // Initialize the base distinguisher plugin
        auto base_init_result = DistinguisherPlugin::Init(args);
        if (base_init_result.IsError()) {
            return base_init_result.Error();
        }

        // Model generated with single plaintext cause the modelled value to be constant
        // under the same key hypothesis, as such the variance is null and therefore
        // the pearson correlation coefficient is ill-defined. 
        if (m_Dataset->GetHeader().PlaintextMode == PlaintextGenerationMode::FIXED) {
            METRISCA_ERROR("Pearson correlation coefficient is ill-defined for constant plaintext mode (stability issue)");
            return Error::UNSUPPORTED_OPERATION;
        }

        // Otherwise simply return success
        return {};
    }

    Result<std::vector<std::pair<uint32_t, Matrix<double>>>, Error> PearsonDistinguisher::Distinguish()
    {
        auto result = InitializeResultMatrices();

        auto model_or_error = m_PowerModel->Model();
        if(model_or_error.IsError())
            return model_or_error.Error();
        auto model = model_or_error.Value();

        // Display the indicator
        indicators::BlockProgressBar progress_bar{
            indicators::option::BarWidth(60),
            indicators::option::PrefixText("Computing pearson correlation coefficients"),
            indicators::option::MaxProgress(m_SampleCount),
            indicators::option::Start{"["},
            indicators::option::End{"]"},
            indicators::option::ShowElapsedTime{ true },
            indicators::option::ShowRemainingTime{ true }
        };

        // Compute the pearson correlation coefficient between the model and the sample.
        // This operation is performed on every sample in the range.
        for(uint32_t s = m_SampleStart; s < m_SampleStart + m_SampleCount; s++)
        {
            if (s % (m_SampleCount / 200 + 1) == 0) {
                progress_bar.set_progress((float) s);
            }

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

                    if (divisor <= 1e-9 && k == 0 && step == 0) { // Only print for first key (to prevent 256 message for each sample)
                        METRISCA_WARN("Null variance for sample {}, this can be caused by fixed plaintext accross all samples", s);
                    }
                    
                    result[step].second(k, s - m_SampleStart) = std::abs(pearson_coeff);
                    trace_count_start = step_trace_count;
                }
            }
        }

        // Complete progress bar
        progress_bar.mark_as_completed();

        return result;
    }

}
