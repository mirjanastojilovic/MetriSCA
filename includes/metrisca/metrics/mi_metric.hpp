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

    class MIMetric : public MetricPlugin {
    public:
        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<void, Error> Compute() override;

    private:
        std::shared_ptr<TraceDataset> m_Dataset{ nullptr };
        std::shared_ptr<ProfilerPlugin> m_Profiler{ nullptr };
        double m_IntegrationLowerBound{};
        double m_IntegrationUpperBound{};
        uint32_t m_IntegrationSamples{};
        double m_Sigma{};
    };

}
