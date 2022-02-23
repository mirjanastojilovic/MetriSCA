/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/ttest_metric.hpp"

#include "metrisca/core/csv_writer.hpp"
#include "metrisca/core/errors.hpp"
#include "metrisca/core/matrix.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/utils/numerics.hpp"

#include <vector>
#include <string>
#include <array>

namespace metrisca {

    Result<void, Error> TTestMetric::Init(const ArgumentList& args)
    {
        auto base_result = MetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result.Error();

        auto fixed_dataset = args.GetDataset(ARG_NAME_FIXED_DATASET);
        auto random_dataset = args.GetDataset(ARG_NAME_RANDOM_DATASET);
        auto trace_count = args.GetUInt32(ARG_NAME_TRACE_COUNT);
        auto trace_step = args.GetUInt32(ARG_NAME_TRACE_STEP);
        auto sample_start = args.GetUInt32(ARG_NAME_SAMPLE_START);
        auto sample_end = args.GetUInt32(ARG_NAME_SAMPLE_END);

        if(!fixed_dataset.has_value())
            return Error::MISSING_ARGUMENT;

        if(!random_dataset.has_value())
            return Error::MISSING_ARGUMENT;

        m_FixedDataset = fixed_dataset.value();
        m_RandomDataset = random_dataset.value();

        auto fixed_header = m_FixedDataset->GetHeader();
        auto random_header = m_RandomDataset->GetHeader();

        if(fixed_header.NumberOfTraces != random_header.NumberOfTraces)
            return Error::INVALID_ARGUMENT;

        if(fixed_header.NumberOfSamples != random_header.NumberOfSamples)
            return Error::INVALID_ARGUMENT;

        m_TraceCount = trace_count.value_or(fixed_header.NumberOfTraces);
        m_TraceStep = trace_step.value_or(0);
        m_SampleStart = sample_start.value_or(0);
        m_SampleCount = sample_end.value_or(fixed_header.NumberOfSamples) - m_SampleStart;

        if(m_SampleCount == 0)
            return Error::INVALID_ARGUMENT;

        if(m_SampleStart + m_SampleCount > fixed_header.NumberOfSamples)
            return Error::INVALID_ARGUMENT;

        if(m_TraceCount > fixed_header.NumberOfTraces)
            return Error::INVALID_ARGUMENT;

        return {};
    }

    Result<void, Error> TTestMetric::Compute()
    {
        std::vector<uint32_t> trace_counts = { m_TraceCount };
        if(m_TraceStep > 0)
        {
            trace_counts = numerics::ARange(m_TraceStep, m_TraceCount+1, m_TraceStep);
        }

        Matrix<double> tvalues(m_SampleCount, trace_counts.size());
        for(uint32_t s = m_SampleStart; s < m_SampleStart + m_SampleCount; s++)
        {
            for(uint32_t step = 0; step < trace_counts.size(); step++)
            {
                uint32_t trace_count = trace_counts[step];
                
                tvalues(step, s - m_SampleStart) = (numerics::WelchTTest(m_FixedDataset->GetSample(s).subspan(0, trace_count), m_RandomDataset->GetSample(s).subspan(0, trace_count)));
            }
        }

        // Write CSV header
        CSVWriter writer(m_OutputFile);
        writer << "trace_count";
        for(uint32_t s = m_SampleStart; s < m_SampleStart + m_SampleCount; s++)
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

        return {};
    }

}