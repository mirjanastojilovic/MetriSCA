/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/profilers/profiler.hpp"

#include "metrisca/core/trace_dataset.hpp"

namespace metrisca {

    Result<void, Error> ProfilerPlugin::Init(const ArgumentList& args)
    {
        auto dataset = args.GetDataset(ARG_NAME_DATASET);
        auto known_key = args.GetUInt8(ARG_NAME_KNOWN_KEY);
        auto byte_index = args.GetUInt32(ARG_NAME_BYTE_INDEX);

        if(!dataset.has_value())
            return Error::MISSING_ARGUMENT;

        if(!known_key.has_value())
            return Error::MISSING_ARGUMENT;

        m_Dataset = dataset.value();
        m_KnownKey = known_key.value();

        TraceDatasetHeader header = m_Dataset->GetHeader();
        m_ByteIndex = byte_index.value_or(0);

        if(m_ByteIndex >= header.PlaintextSize)
            return Error::INVALID_ARGUMENT;

        return {};
    }

}