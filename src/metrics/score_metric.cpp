/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "score_metric.hpp"

#include "core/csv_writer.hpp"
#include "core/errors.hpp"
#include "core/matrix.hpp"
#include "core/trace_dataset.hpp"
#include "app/cli_view.hpp"
#include "app/cli_model.hpp"
#include "utils/numerics.hpp"

#include <vector>
#include <string>
#include <array>
#include <nonstd/span.hpp>

namespace metrisca {

    struct ScoreArguments {
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

    static int parseScoreArguments(ScoreArguments& out, const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        ScoreArguments result = {};

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
            view.PrintError("Missing dataset parameter. See 'help metric score'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!modelParam) 
        {
            view.PrintError("Missing model parameter. See 'help metric score'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!distinguisherParam)
        {
            view.PrintError("Missing distinguisher parameter. See 'help metric score'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!outParam)
        {
            view.PrintError("Missing out parameter. See 'help metric score'.\n");
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

    int metrics::ScoreMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        ScoreArguments args;
        int argParseResult = parseScoreArguments(args, view, model, context);
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

            int modelResult = powermodel(modelOutput, dataset, args.ByteIndex);
            if(modelResult != SCA_OK)
            {
                view.PrintError("Model failed with error code " + std::to_string(modelResult) + ": " + ErrorCause(modelResult) + ".\n");
                return modelResult;
            }
        }

        view.PrintInfo("Attacking dataset for byte " + std::to_string(args.ByteIndex) + " from samples [" + std::to_string(args.SampleStart) + ", " + std::to_string(args.SampleEnd) + ") using " + std::to_string(args.TraceCount) + " traces...");

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
            writer << "score_key" + std::to_string(i);
        }
        writer << csv::EndRow;

        // Write CSV data
        // First column is number of traces, then key max scores
        for(size_t step = 0; step < distinguisherOutput.size(); step++)
        {
            uint32_t stepCount = distinguisherOutput[step].first;
            writer << stepCount;

            const Matrix<double>& scores = distinguisherOutput[step].second;
            for(uint32_t k = 0; k < 256; k++)
            {
                const nonstd::span<const double> keyScore = scores.GetRow(k);
                double maxScore = numerics::Max(keyScore);
                writer << maxScore;
            }

            writer << csv::EndRow;
        }

        view.PrintInfo("");

        return SCA_OK;
    }

}