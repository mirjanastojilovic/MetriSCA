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
#include <string>

namespace metrisca {

    class RankEstimationMetric : public BasicMetricPlugin {
    public:

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<void, Error> Compute() override;

    private:
        std::shared_ptr<ScorePlugin> m_ScorePlugin{};
        std::string m_Key;
        uint32_t m_BinCount{};
    };

}
