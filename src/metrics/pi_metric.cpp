/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "pi_metric.hpp"

#include "core/errors.hpp"
#include "core/csv_writer.hpp"
#include "app/cli_view.hpp"
#include "app/cli_model.hpp"
#include "utils/math.hpp"
#include "utils/numerics.hpp"
#include "utils/crypto.hpp"

#include <array>
#include <vector>

namespace metrisca {

    struct PIArguments {
        std::string TrainingDatasetAlias;
        std::string TestingDatasetAlias;
        std::string Profiler;
        std::string OutputFile;
        uint32_t ByteIndex;
        uint32_t Key;
        double Sigma;
        bool UseCache;
    };

    static int parsePIArguments(PIArguments& out, const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        PIArguments result = {};

        Parameter trainingDatasetParam = context.GetPositionalParameter(1);
        Parameter testingDatasetParam = context.GetPositionalParameter(2);
        Parameter profilerParam = context.GetNamedParameter({"profiler", "p"});
        Parameter outParam = context.GetNamedParameter({"out", "o"});
        Parameter byteIndexParam = context.GetNamedParameter({"byte", "b"});
        Parameter keyParam = context.GetNamedParameter({"key", "k"});
        Parameter useCacheParam = context.GetNamedParameter({"use-cache", "c"});
        Parameter sigmaParam = context.GetNamedParameter({ "sigma", "sg" });
        
        if(!trainingDatasetParam)
        {
            view.PrintError("Missing training dataset parameter. See 'help metric mi'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!testingDatasetParam)
        {
            view.PrintError("Missing testing dataset parameter. See 'help metric mi'.\n");
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

        trainingDatasetParam.Get(result.TrainingDatasetAlias);
        if(!model.HasDataset(result.TrainingDatasetAlias))
        {
            view.PrintError("Unknown dataset '" + result.TrainingDatasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        testingDatasetParam.Get(result.TestingDatasetAlias);
        if(!model.HasDataset(result.TestingDatasetAlias))
        {
            view.PrintError("Unknown dataset '" + result.TestingDatasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        profilerParam.Get(result.Profiler);
        if(!model.HasProfiler(result.Profiler))
        {
            view.PrintError("Unknown profiler '" + result.Profiler + "'. See 'help profile'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        outParam.Get(result.OutputFile);

        DatasetInfo trainingDatasetInfo = model.GetDatasetInfo(result.TrainingDatasetAlias);
        DatasetInfo testingDatasetInfo = model.GetDatasetInfo(result.TestingDatasetAlias);
        result.ByteIndex = byteIndexParam.GetOrDefault(0);
        result.Key = keyParam.GetOrDefault(0);
        result.UseCache = useCacheParam.GetOrDefault(false);
        result.Sigma = sigmaParam.GetOrDefault((double)0.0);
        
        if(trainingDatasetInfo.Header.KeySize != testingDatasetInfo.Header.KeySize)
        {
            view.PrintError("Key size missmatch. Training dataset keys are of size " + std::to_string(trainingDatasetInfo.Header.KeySize) + 
                " and testing dataset keys are of size " + std::to_string(testingDatasetInfo.Header.KeySize) + ".\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(trainingDatasetInfo.Header.PlaintextSize != testingDatasetInfo.Header.PlaintextSize)
        {
            view.PrintError("Plaintext size missmatch. Training dataset plaintexts are of size " + std::to_string(trainingDatasetInfo.Header.PlaintextSize) + 
                " and testing dataset plaintexts are of size " + std::to_string(testingDatasetInfo.Header.PlaintextSize) + ".\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(result.ByteIndex > trainingDatasetInfo.Header.KeySize || result.ByteIndex > trainingDatasetInfo.Header.PlaintextSize)
        {
            view.PrintError("Invalid byte index. Dataset keys or plaintexts are of size " + std::to_string(trainingDatasetInfo.Header.KeySize) + ".\n");
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

    int metrics::PIMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        PIArguments args;
        int argParseResult = parsePIArguments(args, view, model, context);
        if(argParseResult != SCA_OK)
        {
            return argParseResult;
        }

        const TraceDataset& trainingDataset = model.GetDataset(args.TrainingDatasetAlias);
        const TraceDataset& testingDataset = model.GetDataset(args.TestingDatasetAlias);
        ProfilerFunction profiler = model.GetProfiler(args.Profiler);

        // Attempt to load training profile from file
        bool profileLoaded = false;
        Matrix<double> profile;
        if(args.UseCache)
        {
            std::string profileFilename = args.TrainingDatasetAlias + "_" + args.Profiler + "_k" + std::to_string(args.Key) + "_b" + std::to_string(args.ByteIndex) + ".profile";
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

        // If training profile cannot be loaded from file, compute it
        if(!profileLoaded)
        {
            view.PrintInfo("Computing profile for key " + std::to_string(args.Key) + " and for byte " + std::to_string(args.ByteIndex) + " of dataset '" + args.TrainingDatasetAlias + "'...");

            int profileResult = profiler(profile, trainingDataset, args.Key, args.ByteIndex);
            if(profileResult != SCA_OK)
            {
                view.PrintError("Profiler failed with error code " + std::to_string(profileResult) + ": " + ErrorCause(profileResult) + ".\n");
                return profileResult;
            }
        }

        // If a sigma parameter is specified (greather than zero),
        // override the sigmas loaded from the profile
        if (args.Sigma > 0.0)
        {
            profile.FillRow(1, args.Sigma);
        }

        // Only S-box is currently supported
        if (testingDataset.GetHeader().EncryptionType != EncryptionAlgorithm::S_BOX)
        {
            return SCA_UNSUPPORTED_OPERATION;
        }

        // Find the best testing sample using CPA
        uint32_t testing_trace_count = testingDataset.GetHeader().NumberOfTraces;
        uint32_t testing_sample_count = testingDataset.GetHeader().NumberOfSamples;

        // Compute the identity model for each plaintext and the selected key
        std::vector<uint8_t> y;
        y.reserve(testing_trace_count);
        for(uint32_t t = 0; t < testing_trace_count; t++)
        {
            auto& plaintext = testingDataset.GetPlaintext(t);
            const uint8_t pt = plaintext[args.ByteIndex];
            y.push_back(crypto::SBox(pt ^ args.Key));
        }

        // Compute the correlation between the model and every sample
        std::vector<double> corr;
        corr.reserve(testing_sample_count);
        for(uint32_t s = 0; s < testing_sample_count; s++)
        {
            auto& sample = testingDataset.GetSample(s);
            double pearson = numerics::PearsonCorrelation(y, sample);
            corr.push_back(std::abs(pearson));
        }

        // Best sample is the one with the highest correlation
        size_t best_testing_sample_index = numerics::ArgMax(corr);

        // Partition the traces of the best sample into classes based on the model output
        std::array<std::vector<int32_t>, 256> testing_classes;
        auto& best_testing_sample = testingDataset.GetSample((uint32_t)best_testing_sample_index);
        for(uint32_t t = 0; t < testing_trace_count; t++)
        {
            const uint8_t trace_model = y[t];
            testing_classes[trace_model].push_back(best_testing_sample[t]);
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

        view.PrintInfo("Computing PI...");

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

        view.PrintInfo("Saving metric result to '" + args.OutputFile + "'...");

        CSVWriter writer(args.OutputFile);
        writer << "pi" << "avg_sigma" << csv::EndRow;
        writer << pi << numerics::Mean(stds) << csv::EndRow;

        view.PrintInfo("");

        return SCA_OK;
    }

}