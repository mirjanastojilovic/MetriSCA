/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "guess_metric.hpp"

#include "core/errors.hpp"
#include "core/csv_writer.hpp"
#include "app/cli_view.hpp"
#include "app/cli_model.hpp"
#include "utils/numerics.hpp"

namespace metrisca {

    struct GuessArguments {
        std::string DatasetAlias;
        std::string Model;
        std::string Distinguisher;
        std::string OutputFile;
        uint32_t ByteIndex;
        uint32_t TraceCount;
        uint32_t TraceStep;
        uint32_t SampleStart;
        uint32_t SampleEnd;
        bool UseCache;
    };

    static int parseGuessArguments(GuessArguments& out, const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        GuessArguments result = {};

        Parameter datasetParam = context.GetPositionalParameter(1);
        Parameter modelParam = context.GetNamedParameter({"model", "m"});
        Parameter distinguisherParam = context.GetNamedParameter({"distinguisher", "d"});
        Parameter outParam = context.GetNamedParameter({"out", "o"});
        Parameter byteIndexParam = context.GetNamedParameter({"byte", "b"});
        Parameter traceCountParam = context.GetNamedParameter({"traces", "t"});
        Parameter traceStepParam = context.GetNamedParameter({"step", "ts"});
        Parameter sampleStartParam = context.GetNamedParameter({"start", "s"});
        Parameter sampleEndParam = context.GetNamedParameter({"end", "e"});
        Parameter useCacheParam = context.GetNamedParameter({"use-cache", "c"});

        if(!datasetParam)
        {
            view.PrintError("Missing dataset parameter. See 'help metric guess'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!modelParam) 
        {
            view.PrintError("Missing model parameter. See 'help metric guess'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!distinguisherParam)
        {
            view.PrintError("Missing distinguisher parameter. See 'help metric guess'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!outParam)
        {
            view.PrintError("Missing out parameter. See 'help metric guess'.\n");
            return SCA_INVALID_COMMAND;
        }

        datasetParam.Get(result.DatasetAlias);
        if(!model.HasDataset(result.DatasetAlias))
        {
            view.PrintError("Unknown dataset '" + result.DatasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        modelParam.Get(result.Model);
        if(!model.HasModel(result.Model))
        {
            view.PrintError("Unknown model '" + result.Model + "'. See 'help model'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        distinguisherParam.Get(result.Distinguisher);
        if(!model.HasDistinguisher(result.Distinguisher))
        {
            view.PrintError("Unknown distinguisher '" + result.Distinguisher + "'. See 'help metric'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        outParam.Get(result.OutputFile);

        DatasetInfo datasetInfo = model.GetDatasetInfo(result.DatasetAlias);
        result.ByteIndex = byteIndexParam.GetOrDefault(0);
        result.TraceCount = traceCountParam.GetOrDefault(datasetInfo.Header.NumberOfTraces);
        result.TraceStep = traceStepParam.GetOrDefault(0);
        result.SampleStart = sampleStartParam.GetOrDefault(0);
        result.SampleEnd = sampleEndParam.GetOrDefault(datasetInfo.Header.NumberOfSamples);
        result.UseCache = useCacheParam.GetOrDefault(false);

        if(result.SampleStart >= result.SampleEnd || result.SampleStart >= datasetInfo.Header.NumberOfSamples)
        {
            view.PrintError("Invalid start sample index. Dataset has " + std::to_string(datasetInfo.Header.NumberOfSamples) + " samples.\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(result.SampleEnd <= result.SampleStart || result.SampleEnd > datasetInfo.Header.NumberOfSamples)
        {
            view.PrintError("Invalid end sample index. Dataset has " + std::to_string(datasetInfo.Header.NumberOfSamples) + " samples.\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(result.TraceCount > datasetInfo.Header.NumberOfTraces)
        {
            view.PrintError("Invalid number of traces. Dataset has " + std::to_string(datasetInfo.Header.NumberOfTraces) + " traces.\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(result.ByteIndex >= datasetInfo.Header.PlaintextSize)
        {
            view.PrintError("Invalid byte index. Dataset plaintexts are of size " + std::to_string(datasetInfo.Header.PlaintextSize) + ".\n");
            return SCA_INVALID_ARGUMENT;
        }

        out = result;

        return SCA_OK;
    }

    int metrics::GuessMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        GuessArguments args;
        int argParseResult = parseGuessArguments(args, view, model, context);
        if(argParseResult != SCA_OK)
        {
            return argParseResult;
        }

        const TraceDataset& dataset = model.GetDataset(args.DatasetAlias);
        ModelFunction powermodel = model.GetModel(args.Model);
        DistinguisherFunction distinguisher = model.GetDistinguisher(args.Distinguisher);

        bool modelLoaded = false;
        Matrix<int> modelOutput;
        if(args.UseCache)
        {
            // Try to load the model from the disk
            std::string modelFileName = args.DatasetAlias + "_" + args.Model + "_b" + std::to_string(args.ByteIndex) + ".model";
            int modelLoadResult = modelOutput.LoadFromFile(modelFileName);
            if(modelLoadResult != SCA_OK)
            {
                view.PrintWarning("Failed to load model from file '" + modelFileName + "' with code (" + std::to_string(modelLoadResult) + "): " + ErrorCause(modelLoadResult));
            }
            else
            {
                view.PrintInfo("Loaded model from file '" + modelFileName + "'");
                modelLoaded = true;
            }
        }

        if(!modelLoaded)
        {
            view.PrintInfo("Computing model for byte " + std::to_string(args.ByteIndex) + " of dataset '" + args.DatasetAlias + "'...");

            // Compute the power model
            int modelResult = powermodel(modelOutput, dataset, args.ByteIndex);
            if(modelResult != SCA_OK)
            {
                view.PrintError("Model failed with error code " + std::to_string(modelResult) + ": " + ErrorCause(modelResult) + ".\n");
                return modelResult;
            }
        }

        view.PrintInfo("Attacking dataset for byte " + std::to_string(args.ByteIndex) + " from samples [" + std::to_string(args.SampleStart) + ", " + std::to_string(args.SampleEnd) + ") using " + std::to_string(args.TraceCount) + " traces...");

        // Compute the key scores
        std::vector<std::pair<uint32_t, Matrix<double>>> distinguisherOutput;
        int distinguisherResult = distinguisher(distinguisherOutput, dataset, modelOutput, args.SampleStart, args.SampleEnd, args.TraceCount, args.TraceStep);
        if(distinguisherResult != SCA_OK)
        {
            view.PrintError("Distinguisher failed with error code " + std::to_string(distinguisherResult) + ": " + ErrorCause(distinguisherResult) + ".\n");
            return distinguisherResult;
        }
    
        view.PrintInfo("Saving metric result to '" + args.OutputFile + "'...");

        // Write CSV header
        CSVWriter writer(args.OutputFile);
        writer << "trace_count";
        for(uint32_t i = 0; i < 256; ++i)
        {
            writer << "key_guess" + std::to_string(i+1);
        }
        writer << csv::EndRow;

        // Write CSV data
        // First column is number of traces, then key guess
        for(size_t step = 0; step < distinguisherOutput.size(); step++)
        {
            uint32_t stepCount = distinguisherOutput[step].first;
            writer << stepCount;

            const Matrix<double>& scores = distinguisherOutput[step].second;
            std::vector<std::pair<uint32_t, double>> key_max_scores;
            key_max_scores.reserve(256);
            for(uint32_t k = 0; k < 256; k++)
            {
                const auto keyScore = scores.GetRow(k);
                double maxScore = numerics::Max(keyScore);
                key_max_scores.emplace_back(k, maxScore);
            }
            std::sort(key_max_scores.begin(), key_max_scores.end(), [](auto& a, auto& b) 
            {
                return a.second > b.second;
            });

            for(const auto& score : key_max_scores)
            {
                writer << score.first;
            }

            writer << csv::EndRow;
        }

        view.PrintInfo("");

        return SCA_OK;
    }

}