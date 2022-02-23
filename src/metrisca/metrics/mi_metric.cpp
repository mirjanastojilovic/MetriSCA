/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/mi_metric.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/profilers/profiler.hpp"

#include <array>
#include <vector>
#include <limits>

namespace metrisca {

    Result<void, int> MIMetric::Init(const ArgumentList& args)
    {
        auto base_result = MetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result.Error();

        auto dataset = args.GetDataset(ARG_NAME_DATASET);
        auto profiler = args.GetString(ARG_NAME_PROFILER);
        auto lower_bound = args.GetDouble(ARG_NAME_INTEGRATION_LOWER_BOUND);
        auto upper_bound = args.GetDouble(ARG_NAME_INTEGRATION_UPPER_BOUND);
        auto samples = args.GetUInt32(ARG_NAME_INTEGRATION_SAMPLE_COUNT);
        auto sigma = args.GetDouble(ARG_NAME_SIGMA);

        if(!dataset.has_value())
            return SCA_MISSING_ARGUMENT;

        if(!profiler.has_value())
            return SCA_MISSING_ARGUMENT;

        m_Dataset = dataset.value();

        auto profiler_or_error = PluginFactory::The().ConstructAs<ProfilerPlugin>(PluginType::Profiler, profiler.value(), args);
        if(profiler_or_error.IsError())
            return profiler_or_error.Error();

        m_Profiler = profiler_or_error.Value();

        m_IntegrationLowerBound = lower_bound.value_or(std::numeric_limits<double>::lowest());
        m_IntegrationUpperBound = upper_bound.value_or(std::numeric_limits<double>::max());
        m_IntegrationSamples = samples.value_or(0);
        m_Sigma = sigma.value_or(0.0);

        return {};
    }

    Result<void, int> MIMetric::Compute()
    {
        auto profiler_or_error = m_Profiler->Profile();
        if(profiler_or_error.IsError())
            return profiler_or_error.Error();

        auto profile = profiler_or_error.Value();

        // If a sigma parameter is specified (greather than zero),
        // override the sigmas loaded from the profile
        if(m_Sigma > 0.0)
        {
            profile.FillRow(1, m_Sigma);
        }

        const auto& means = profile.GetRow(0);
        const auto& stds = profile.GetRow(1);

        // Compute the truncation bounds of our indefinite integral.
        // As the function is a sum of gaussian curves, we assume that
        // we can truncate the interval based on the curves with highest
        // and lowest means. However, this might be a wrong assumption.
        auto [minMean, maxMean] = numerics::MinMax(means);
        size_t minMeanIndex = numerics::ArgMin(means);
        size_t maxMeanIndex = numerics::ArgMax(means);
        double minMeanStd = stds[minMeanIndex];
        double maxMeanStd = stds[maxMeanIndex];

        double std_width_factor = 4.0;
        double a = minMean - (std_width_factor * minMeanStd);
        double b = maxMean + (std_width_factor * maxMeanStd);
        double minStd = numerics::Min(stds);
        double min_width = 2.0 * std_width_factor * minStd;
        // Number of evaluation samples per unit interval.
        // This is computed so that the curve with the minimum width has
        // 100 sample points
        double samples_per_unit = 100.0 / min_width;
        uint32_t n = (uint32_t)std::round((b - a) * samples_per_unit);

        // Override integration parameters if provided by the user
        if(m_IntegrationLowerBound != std::numeric_limits<double>::lowest())
        {
            if(a != m_IntegrationLowerBound)
            {
                //view.PrintWarning("Using user-defined integration lower bound. Optimal lower bound is a=" + std::to_string(a) + ".");
            }
            a = m_IntegrationLowerBound;
        }

        if(m_IntegrationUpperBound != std::numeric_limits<double>::max())
        {
            if(b != m_IntegrationUpperBound)
            {
                //view.PrintWarning("Using user-defined integration upper bound. Optimal upper bound is b=" + std::to_string(b) + ".");
            }
            b = m_IntegrationUpperBound;
        }

        if(m_IntegrationSamples > 0)
        {
            if(n != m_IntegrationSamples)
            {
                //view.PrintWarning("Using user-defined integration sample count. Optimal count is n=" + std::to_string(n) + ".");
            }
            n = m_IntegrationSamples;
        }

        // Validate the integration parameters
        if(n == 0)
        {
            return SCA_INVALID_DATA;
        }
        else if(n > 99999)
        {
            n = 99999;
            //view.PrintWarning("Number of integral samples is too large (" + std::to_string(n) + "). Falling back to a default value (99999).");
        }

        //view.PrintInfo("Evaluating integral from a=" + std::to_string(a) + " to b=" + std::to_string(b) + " with " + std::to_string(n) + " samples...");

        // Sample each individual gaussian functions with their respective parameters
        std::array<std::vector<double>, 256> samples;
        double delta;
        for(uint32_t k = 0; k < 256; k++)
        {
            delta = numerics::SampleGaussian(samples[k], means[k], stds[k], a, b, n);
        }
        size_t real_n = samples[0].size(); // n must be odd

        // Compute the sum of all samples
        std::vector<double> sum_samples(real_n);
        for(uint32_t k = 0; k < 256; k++)
        {
            for(uint32_t i = 0; i < real_n; i++)
            {
                sum_samples[i] += samples[k][i];
            }
        }

        // Compute the log base 2 of each sample divided by the sum of samples
        std::array<std::vector<double>, 256> log_samples;
        for(uint32_t k = 0; k < 256; k++)
        {
            log_samples[k].reserve(real_n);
            for(uint32_t i = 0; i < real_n; i++)
            {
                double sample = samples[k][i];
                double value = 0.0;
                double min = std::numeric_limits<double>::min();
                if(sample >= min)
                {
                    value = std::log2(sample / sum_samples[i]);
                }
                log_samples[k].push_back(value);
            }
        }

        // Compute the final function samples
        std::array<std::vector<double>, 256> final_samples;
        for(uint32_t k = 0; k < 256; k++)
        {
            final_samples[k].reserve(real_n);
            for(uint32_t i = 0; i < real_n; i++)
            {
                final_samples[k].push_back(samples[k][i] * log_samples[k][i]);
            }
        }

        // Compute the integral of the samples for each key using Simpson's rule
        std::array<double, 256> integrals = std::array<double, 256>();
        for(uint32_t k = 0; k < 256; k++)
        {
            integrals[k] = numerics::Simpson(final_samples[k], delta);
        }

        // Compute the final mi value
        double mi = 8.0;
        double one_over_keyset = 1.0 / 256.0;
        for(uint32_t k = 0; k < 256; k++)
        {
            mi += one_over_keyset * integrals[k];
        }

        //view.PrintInfo("Saving metric result to '" + args.OutputFile + "'...");

        CSVWriter writer(m_OutputFile);
        writer << "mi" << "avg_sigma" << csv::EndRow;
        writer << mi << numerics::Mean(stds) <<  csv::EndRow;

        return {};
    }

}