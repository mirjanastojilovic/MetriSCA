/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/scores/score.hpp"

#include "metrisca/core/logger.hpp"
#include "metrisca/core/trace_dataset.hpp"

#include <exception>

namespace metrisca {
    Result<void, Error> ScorePlugin::Init(const ArgumentList& args)
    {
        // Retrieve the dataset
        auto dataset = args.GetDataset(ARG_NAME_DATASET);
        if (!dataset.has_value()) {
            METRISCA_ERROR("Missing argument: {}", ARG_NAME_DATASET);
            return Error::MISSING_ARGUMENT;
        }

        m_Dataset = std::move(dataset.value());

        // Retrieve sample & trace parameters
        auto sample_start = args.GetUInt32(ARG_NAME_SAMPLE_START);
        auto sample_end = args.GetUInt32(ARG_NAME_SAMPLE_END);
        auto trace_count = args.GetUInt32(ARG_NAME_TRACE_COUNT);
        auto trace_step = args.GetUInt32(ARG_NAME_TRACE_STEP);
        TraceDatasetHeader header = m_Dataset->GetHeader();

        m_TraceCount = trace_count.value_or(header.NumberOfTraces);
        m_TraceStep = trace_step.value_or(0);
        m_SampleStart = sample_start.value_or(0);
        m_SampleCount = sample_end.value_or(header.NumberOfSamples) - m_SampleStart;
        
        // Sanity checks
        if(m_TraceCount == 0) {
            METRISCA_ERROR("Requires trace-count to be at least 1");
            return Error::INVALID_ARGUMENT;
        }


        if (m_SampleCount - m_SampleStart > header.NumberOfSamples) {
            METRISCA_ERROR("There are not enough samples in the dataset");
            return Error::INVALID_ARGUMENT;
        }

        if (m_TraceCount > header.NumberOfTraces) {
            METRISCA_ERROR("Trace-count must be smaller than the number of traces in the dataset");
            return Error::INVALID_ARGUMENT;
        }

        if (m_TraceStep >= m_TraceCount) {
            METRISCA_ERROR("Trace-step must be smaller than the trace-count");
            return Error::INVALID_ARGUMENT;
        }

        // Otherwise, we're goutchi
        return {};
    }
}
