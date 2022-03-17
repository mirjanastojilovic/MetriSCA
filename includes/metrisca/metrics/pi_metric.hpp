/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metric.hpp"

namespace metrisca {

    class PIMetric : public MetricPlugin {
    public:

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<void, Error> Compute() override;

    private:
        std::shared_ptr<TraceDataset> m_TrainingDataset{ nullptr };
        std::shared_ptr<TraceDataset> m_TestingDataset{ nullptr };
        std::shared_ptr<ProfilerPlugin> m_Profiler{ nullptr };
        uint32_t m_ByteIndex{};
        uint8_t m_KnownKey{};
        double m_Sigma{};
    };

}
