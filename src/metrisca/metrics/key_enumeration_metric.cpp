/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/key_enumeration_metric.hpp"

#include "metrisca/core/errors.hpp"
#include "metrisca/core/logger.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/profilers/profiler.hpp"
#include "metrisca/utils/numerics.hpp"
#include "metrisca/utils/crypto.hpp"

#define DEFAULT_KEY_COUNT 4096

namespace metrisca {

    Result<void, Error> KeyEnumerationMetric::Init(const ArgumentList& args)
    {
        // Retrieve the number of key to enumerate
        m_KeyCount = args.GetUInt32(ARG_NAME_KEY_COUNT)
                         .value_or(DEFAULT_KEY_COUNT);

        // Retrieve the dataset and the traceCount / traceStep
        auto dataset_arg = args.GetDataset(ARG_NAME_DATASET);
        auto trace_count_arg = args.GetUInt32(ARG_NAME_TRACE_COUNT);
        auto trace_step_arg = args.GetUInt32(ARG_NAME_TRACE_STEP);

        if (!dataset_arg.has_value())
        {
            METRISCA_ERROR("KeyEnumerationMetric require argument {} to be specified but was not provided",
                           ARG_NAME_DATASET);
            return Error::MISSING_ARGUMENT;
        }

        m_Dataset = dataset_arg.value();
        auto dataset_header = m_Dataset->GetHeader();

        m_TraceCount = trace_count_arg.value_or(dataset_header.NumberOfTraces);
        m_TraceStep = trace_step_arg.value_or(0);

        if (m_TraceCount > dataset_header.NumberOfTraces) {
            METRISCA_ERROR("User-specified number of traces ({}) is greater than the number of traces within the database ({})",
                           m_TraceCount,
                           dataset_header.NumberOfTraces);
            return Error::INVALID_ARGUMENT;
        }

        if (m_TraceCount <= 0) {
            METRISCA_ERROR("KeyEnumerationMetric require at least one trace");
            return Error::INVALID_ARGUMENT;
        }

        if (m_TraceStep >= m_TraceCount) {
            METRISCA_ERROR("User-specified number of step exceed the configured number of traces");
            return Error::INVALID_ARGUMENT;
        }

        // Logging duty
        METRISCA_INFO("KeyEnumerationMetric configured to enumerate at most {} keys on at most {} traces in {} step",
                      m_KeyCount,
                      m_TraceCount,
                      m_TraceStep);

        // Return success of initialization operation
        return {};
    }

    Result<void, Error> KeyEnumerationMetric::Compute()
    {
        // Trivial computation
        return {};
    }

}
