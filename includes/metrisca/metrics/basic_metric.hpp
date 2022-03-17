/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/forward.hpp"
#include "metrisca/metrics/metric.hpp"

namespace metrisca {

    class BasicMetricPlugin : public MetricPlugin {
    public:
        virtual ~BasicMetricPlugin() = default;

        virtual Result<void, Error> Init(const ArgumentList& args) override;

    protected:
        std::shared_ptr<TraceDataset> m_Dataset{ nullptr };
        std::shared_ptr<DistinguisherPlugin> m_Distinguisher{ nullptr };
    };

}