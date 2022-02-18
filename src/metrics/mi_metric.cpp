/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "mi_metric.hpp"

#include "core/errors.hpp"
#include "core/csv_writer.hpp"
#include "app/cli_view.hpp"
#include "app/cli_model.hpp"
#include "utils/math.hpp"
#include "utils/numerics.hpp"

#include <array>
#include <vector>

namespace metrisca {

    struct MIArguments {
        std::string DatasetAlias;
        std::string Profiler;
        std::string OutputFile;
        uint32_t ByteIndex;
        uint32_t Key;
        double IntegrationLowerBound;
        double IntegrationUpperBound;
        uint32_t IntegrationSamples;
        double Sigma;
        bool UseCache;
    };

    static int parseMIArguments(MIArguments& out, const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        MIArguments result = {};

        Parameter datasetParam = context.GetPositionalParameter(1);
        Parameter profilerParam = context.GetNamedParameter({"profiler", "p"});
        Parameter outParam = context.GetNamedParameter({"out", "o"});
        Parameter byteIndexParam = context.GetNamedParameter({"byte", "b"});
        Parameter keyParam = context.GetNamedParameter({"key", "k"});
        Parameter useCacheParam = context.GetNamedParameter({"use-cache", "c"});
        Parameter integrationLowerBoundParam = context.GetNamedParameter({"lower", "l"});
        Parameter integrationUpperBoundParam = context.GetNamedParameter({"upper", "u"});
        Parameter integrationSamplesParam = context.GetNamedParameter({"samples", "s"});
        Parameter sigmaParam = context.GetNamedParameter({"sigma", "sg"});

        if(!datasetParam)
        {
            view.PrintError("Missing dataset parameter. See 'help metric mi'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!profilerParam) 
        {
            view.PrintError("Missing profiler parameter. See 'help metric mi'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!outParam)
        {
            view.PrintError("Missing out parameter. See 'help metric mi'.\n");
            return SCA_INVALID_COMMAND;
        }

        datasetParam.Get(result.DatasetAlias);
        if(!model.HasDataset(result.DatasetAlias))
        {
            view.PrintError("Unknown dataset '" + result.DatasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        profilerParam.Get(result.Profiler);
        if(!model.HasProfiler(result.Profiler))
        {
            view.PrintError("Unknown profiler '" + result.Profiler + "'. See 'help profile'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        outParam.Get(result.OutputFile);

        DatasetInfo datasetInfo = model.GetDatasetInfo(result.DatasetAlias);
        result.ByteIndex = byteIndexParam.GetOrDefault(0);
        result.Key = keyParam.GetOrDefault(0);
        result.UseCache = useCacheParam.GetOrDefault(false);
        result.IntegrationLowerBound = integrationLowerBoundParam.GetOrDefault(std::numeric_limits<double>::lowest());
        result.IntegrationUpperBound = integrationUpperBoundParam.GetOrDefault(std::numeric_limits<double>::max());
        result.IntegrationSamples = integrationSamplesParam.GetOrDefault((uint32_t)0);
        result.Sigma = sigmaParam.GetOrDefault((double)0.0);

        if(result.ByteIndex >= datasetInfo.Header.PlaintextSize)
        {
            view.PrintError("Invalid byte index. Dataset plaintexts are of size " + std::to_string(datasetInfo.Header.PlaintextSize) + ".\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(result.Key >= 256)
        {
            view.PrintError("Invalid key. Key must be in the range [0,255].\n");
            return SCA_INVALID_ARGUMENT;
        }

        out = result;

        return SCA_OK;
    }

    int metrics::MIMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        MIArguments args;
        int argParseResult = parseMIArguments(args, view, model, context);
        if(argParseResult != SCA_OK)
        {
            return argParseResult;
        }

        const TraceDataset& dataset = model.GetDataset(args.DatasetAlias);
        ProfilerFunction profiler = model.GetProfiler(args.Profiler);

        bool profileLoaded = false;
        Matrix<double> profile;
        if(args.UseCache)
        {
            std::string profileFilename = args.DatasetAlias + "_" + args.Profiler + "_k" + std::to_string(args.Key) + "_b" + std::to_string(args.ByteIndex) + ".profile";
            int profileLoadResult = profile.LoadFromFile(profileFilename);
            if(profileLoadResult != SCA_OK)
            {
                view.PrintWarning("Failed to load profile from file '" + profileFilename + "' with code (" + std::to_string(profileLoadResult) + "): " + ErrorCause(profileLoadResult));
            }
            else
            {
                view.PrintInfo("Loaded profile from file '" + profileFilename + "'");
                profileLoaded = true;
            }
        }

        if(!profileLoaded)
        {
            view.PrintInfo("Computing profile for key " + std::to_string(args.Key) + " and for byte " + std::to_string(args.ByteIndex) + " of dataset '" + args.DatasetAlias + "'...");

            int profileResult = profiler(profile, dataset, args.Key, args.ByteIndex);
            if(profileResult != SCA_OK)
            {
                view.PrintError("Profiler failed with error code " + std::to_string(profileResult) + ": " + ErrorCause(profileResult) + ".\n");
                return profileResult;
            }
        }

        view.PrintInfo("Computing MI...");

        // If a sigma parameter is specified (greather than zero),
        // override the sigmas loaded from the profile
        if(args.Sigma > 0.0)
        {
            profile.FillRow(1, args.Sigma);
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
        if(args.IntegrationLowerBound != std::numeric_limits<double>::lowest())
        {
            if(a != args.IntegrationLowerBound)
            {
                view.PrintWarning("Using user-defined integration lower bound. Optimal lower bound is a=" + std::to_string(a) + ".");
            }
            a = args.IntegrationLowerBound;
        }

        if(args.IntegrationUpperBound != std::numeric_limits<double>::max())
        {
            if(b != args.IntegrationUpperBound)
            {
                view.PrintWarning("Using user-defined integration upper bound. Optimal upper bound is b=" + std::to_string(b) + ".");
            }
            b = args.IntegrationUpperBound;
        }

        if(args.IntegrationSamples > 0)
        {
            if(n != args.IntegrationSamples)
            {
                view.PrintWarning("Using user-defined integration sample count. Optimal count is n=" + std::to_string(n) + ".");
            }
            n = args.IntegrationSamples;
        }

        // Validate the integration parameters
        if(n == 0)
        {
            view.PrintError("Number of integral samples is zero. This can be caused by invalid profile standard deviation values.\n");
            return SCA_INVALID_DATA;
        }
        else if(n > 99999)
        {
            n = 99999;
            view.PrintWarning("Number of integral samples is too large (" + std::to_string(n) + "). Falling back to a default value (99999).");
        }

        view.PrintInfo("Evaluating integral from a=" + std::to_string(a) + " to b=" + std::to_string(b) + " with " + std::to_string(n) + " samples...");

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
                if(sample >= std::numeric_limits<double>::min())
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

        view.PrintInfo("Saving metric result to '" + args.OutputFile + "'...");

        CSVWriter writer(args.OutputFile);
        writer << "mi" << "avg_sigma" << csv::EndRow;
        writer << mi << numerics::Mean(stds) <<  csv::EndRow;

        view.PrintInfo("");

        return SCA_OK;
    }

}