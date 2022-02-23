/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "basic_metric.hpp"

namespace metrisca {

    class SuccessRateMetric : public BasicMetricPlugin {
    public:

        virtual Result<void, int> Init(const ArgumentList& args) override;

        virtual Result<void, int> Compute() override;

    private:
        uint8_t m_KnownKey;
        uint8_t m_Order;
    };

}
