/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "basic_metric.hpp"

#include <memory>
#include <mutex>

namespace metrisca {

    class RankEstimationMetric : public BasicMetricPlugin {
    public:

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<void, Error> Compute() override;

    private:
        Result<std::array<double, 256>, Error> ComputeProbabilities(size_t number_of_traces, size_t keyByteIdx);

        std::mutex m_GlobalLock; // global lock

        std::vector<uint8_t> m_Key{};
        uint32_t m_BinCount{};
        uint32_t m_TraceStep{};
        uint32_t m_TraceCount{};
        uint32_t m_SampleStart{};
        uint32_t m_SampleCount{};
    };

}
