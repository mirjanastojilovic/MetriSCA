/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/models/model.hpp"

#include "metrisca/core/logger.hpp"
#include "metrisca/core/trace_dataset.hpp"

#include <exception>

namespace metrisca {

    Result<void, Error> PowerModelPlugin::Init(const ArgumentList& args)
    {
        auto dataset = args.GetDataset(ARG_NAME_DATASET);
        auto byte_index = args.GetUInt32(ARG_NAME_BYTE_INDEX);

        if(!dataset.has_value())
            return Error::MISSING_ARGUMENT;

        m_Dataset = dataset.value();
        TraceDatasetHeader header = m_Dataset->GetHeader();

        m_ByteIndex = byte_index.value_or(0);
        
        if(m_ByteIndex >= header.PlaintextSize)
            return Error::INVALID_ARGUMENT;

        return {};
    }

    void PowerModelPlugin::SetByteIndex(uint32_t byteIndex)
    {
        if (m_Dataset == nullptr) {
            METRISCA_ERROR("Cannot call SetByteIndex prior to initialization");
            throw std::runtime_error("Cannot call SetByteIndex prior to initialization");
        }

        if (m_ByteIndex < 0 || m_ByteIndex >= m_Dataset->GetHeader().PlaintextSize) {
            METRISCA_ERROR("Cannot set byte index to value: {}, invalid value or out of range", byteIndex);
            throw std::runtime_error("Cannot set byte index to the provided value");
        }

        m_ByteIndex = byteIndex;
    }

}