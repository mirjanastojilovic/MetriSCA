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

    class TTestMetric : public MetricPlugin {
    public:

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<void, Error> Compute() override;

    private:
        std::shared_ptr<TraceDataset> m_RandomDataset{ nullptr };
        std::shared_ptr<TraceDataset> m_FixedDataset{ nullptr };
        uint32_t m_TraceCount{};
        uint32_t m_TraceStep{};
        uint32_t m_SampleStart{};
        uint32_t m_SampleCount{};
    };

}
