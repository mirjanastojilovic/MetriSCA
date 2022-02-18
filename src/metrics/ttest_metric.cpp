/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "ttest_metric.hpp"

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

    struct TTestArguments {
        std::string FixedDatasetAlias;
        std::string RandomDatasetAlias;
        std::string OutputFile;
        uint32_t TraceCount;
        uint32_t TraceStep;
        uint32_t SampleStart;
        uint32_t SampleEnd;
    };

    static int parseTTestArguments(TTestArguments& out, const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        TTestArguments result = {};

        Parameter fixedDatasetParam = context.GetPositionalParameter(1);
        Parameter randomDatasetParam = context.GetPositionalParameter(2);
        Parameter outParam = context.GetNamedParameter({"out", "o"});
        Parameter traceCountParam = context.GetNamedParameter({"traces", "t"});
        Parameter traceStepParam = context.GetNamedParameter({"step", "ts"});
        Parameter sampleStartParam = context.GetNamedParameter({"start", "s"});
        Parameter sampleEndParam = context.GetNamedParameter({"end", "e"});

        if(!fixedDatasetParam)
        {
            view.PrintError("Missing fixed dataset parameter. See 'help metric ttest'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!randomDatasetParam) 
        {
            view.PrintError("Missing random dataset parameter. See 'help metric ttest'.\n");
            return SCA_INVALID_COMMAND;
        }

        if(!outParam)
        {
            view.PrintError("Missing out parameter. See 'help metric ttest'.\n");
            return SCA_INVALID_COMMAND;
        }

        fixedDatasetParam.Get(result.FixedDatasetAlias);
        if(!model.HasDataset(result.FixedDatasetAlias))
        {
            view.PrintError("Unknown dataset '" + result.FixedDatasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        randomDatasetParam.Get(result.RandomDatasetAlias);
        if(!model.HasDataset(result.RandomDatasetAlias))
        {
            view.PrintError("Unknown dataset '" + result.RandomDatasetAlias + "'. See 'help datasets'.\n");
            return SCA_INVALID_ARGUMENT;
        }

        outParam.Get(result.OutputFile);

        DatasetInfo fixedDatasetInfo = model.GetDatasetInfo(result.FixedDatasetAlias);
        DatasetInfo randomDatasetInfo = model.GetDatasetInfo(result.RandomDatasetAlias);
        result.TraceCount = traceCountParam.GetOrDefault(std::min(fixedDatasetInfo.Header.NumberOfTraces, randomDatasetInfo.Header.NumberOfTraces));
        result.TraceStep = traceStepParam.GetOrDefault(0);
        result.SampleStart = sampleStartParam.GetOrDefault(0);
        result.SampleEnd = sampleEndParam.GetOrDefault(fixedDatasetInfo.Header.NumberOfSamples);

        if(result.TraceCount > fixedDatasetInfo.Header.NumberOfTraces ||
            result.TraceCount > randomDatasetInfo.Header.NumberOfTraces)
        {
            view.PrintError("Invalid number of traces. Datasets have " + std::to_string(fixedDatasetInfo.Header.NumberOfTraces) + " and " + std::to_string(randomDatasetInfo.Header.NumberOfTraces) + " traces.\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(result.SampleStart >= result.SampleEnd || 
            result.SampleStart >= fixedDatasetInfo.Header.NumberOfSamples || 
            result.SampleStart >= randomDatasetInfo.Header.NumberOfSamples)
        {
            view.PrintError("Invalid start sample index. Datasets have " + std::to_string(fixedDatasetInfo.Header.NumberOfSamples) + " and " + std::to_string(randomDatasetInfo.Header.NumberOfSamples) + " samples.\n");
            return SCA_INVALID_ARGUMENT;
        }

        if(result.SampleEnd <= result.SampleStart || 
            result.SampleEnd > fixedDatasetInfo.Header.NumberOfSamples ||
            result.SampleEnd > randomDatasetInfo.Header.NumberOfSamples)
        {
            view.PrintError("Invalid start sample index. Datasets have " + std::to_string(fixedDatasetInfo.Header.NumberOfSamples) + " and " + std::to_string(randomDatasetInfo.Header.NumberOfSamples) + " samples.\n");
            return SCA_INVALID_ARGUMENT;
        }

        out = result;

        return SCA_OK;
    }

    int metrics::TTestMetric(const CLIView& view, const CLIModel& model, const InvocationContext& context)
    {
        TTestArguments args;
        int argParseResult = parseTTestArguments(args, view, model, context);
        if(argParseResult != SCA_OK)
        {
            return argParseResult;
        }

        const TraceDataset& fixedDataset = model.GetDataset(args.FixedDatasetAlias);
        const TraceDataset& randomDataset = model.GetDataset(args.RandomDatasetAlias);

        view.PrintInfo("Computing t-test for samples [" + std::to_string(args.SampleStart) + ", " + std::to_string(args.SampleEnd) + ")...");

        uint32_t sample_count = args.SampleEnd - args.SampleStart;
        std::vector<uint32_t> trace_counts = { args.TraceCount };
        if(args.TraceStep > 0)
        {
            trace_counts = numerics::ARange(args.TraceStep, args.TraceCount+1, args.TraceStep);
        }

        Matrix<double> tvalues(sample_count, trace_counts.size());
        for(uint32_t s = args.SampleStart; s < args.SampleEnd; s++)
        {
            for(uint32_t step = 0; step < trace_counts.size(); step++)
            {
                uint32_t trace_count = trace_counts[step];
                
                tvalues(step, s - args.SampleStart) = (numerics::WelchTTest(fixedDataset.GetSample(s).subspan(0, trace_count), randomDataset.GetSample(s).subspan(0, trace_count)));
            }
        }

        view.PrintInfo("Saving metric result to '" + args.OutputFile + "'...");

        // Write CSV header
        CSVWriter writer(args.OutputFile);
        writer << "trace_count";
        for(uint32_t s = args.SampleStart; s < args.SampleEnd; s++)
        {
            writer << "sample_" + std::to_string(s);
        }
        writer << csv::EndRow;

        // Write CSV data
        // First column is the trace count and next columns are sample t-values
        for(uint32_t step = 0; step < trace_counts.size(); step++)
        {
            uint32_t trace_count = trace_counts[step];
            writer << trace_count;
            writer << tvalues.GetRow(step);
            writer << csv::EndRow;
        }

        view.PrintInfo("");

        return SCA_OK;
    }

}