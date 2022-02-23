/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/distinguishers/distinguisher.hpp"

#include "metrisca/core/matrix.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/models/model.hpp"

namespace metrisca {

    Result<void, int> DistinguisherPlugin::Init(const ArgumentList& args)
    {
        auto dataset = args.GetDataset(ARG_NAME_DATASET);
        auto model = args.GetString(ARG_NAME_MODEL);
        auto sample_start = args.GetUInt32(ARG_NAME_SAMPLE_START);
        auto sample_end = args.GetUInt32(ARG_NAME_SAMPLE_END);
        auto trace_count = args.GetUInt32(ARG_NAME_TRACE_COUNT);
        auto trace_step = args.GetUInt32(ARG_NAME_TRACE_STEP);

        if(!dataset.has_value())
            return SCA_MISSING_ARGUMENT;
        if(!model.has_value())
            return SCA_MISSING_ARGUMENT;

        m_Dataset = dataset.value();
        auto model_or_error = PluginFactory::The().ConstructAs<PowerModelPlugin>(PluginType::PowerModel, model.value(), args);
        if(model_or_error.IsError())
            return model_or_error.Error();
        m_PowerModel = model_or_error.Value();

        TraceDatasetHeader header = m_Dataset->GetHeader();
        m_TraceCount = trace_count.value_or(header.NumberOfTraces);
        m_TraceStep = trace_step.value_or(0);
        m_SampleStart = sample_start.value_or(0);
        m_SampleCount = sample_end.value_or(header.NumberOfSamples) - m_SampleStart;

        if(m_SampleCount == 0)
            return SCA_INVALID_ARGUMENT;

        if(m_SampleStart + m_SampleCount > header.NumberOfSamples)
            return SCA_INVALID_ARGUMENT;

        if(m_TraceCount > header.NumberOfTraces)
            return SCA_INVALID_ARGUMENT;

        return {};
    }

    std::vector<std::pair<uint32_t, Matrix<double>>> DistinguisherPlugin::InitializeResultMatrices()
    {
        std::vector<std::pair<uint32_t, Matrix<double>>> result;
        if(m_TraceStep > 0)
        {
            std::vector<uint32_t> trace_counts = numerics::ARange(m_TraceStep, m_TraceCount + 1, m_TraceStep);
            for(size_t i = 0; i < trace_counts.size(); ++i)
            {
                result.emplace_back(trace_counts[i], Matrix<double>(m_SampleCount, 256));
            }
        }
        else
        {
            result.emplace_back(m_TraceCount, Matrix<double>(m_SampleCount, 256));
        }

        return result;
    }

}