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
#include <unordered_map>

namespace metrisca {

    class KeyEnumerationMetric : public MetricPlugin {
    public:

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<void, Error> Compute() override;

    private:

        std::vector<uint8_t> m_Key{}; /*<! Key of the actual dataset (not the training one) */
        uint32_t m_EnumeratedKeyCount; /*<! Number of key being enumerated by the algorithm */
        uint32_t m_OutputEnumeratedKeyCount; /*<! Number of key being outputted to the file along with their scores */

        std::shared_ptr<TraceDataset> m_Dataset{ nullptr };
        std::shared_ptr<TraceDataset> m_TrainingDataset{ nullptr };

        std::shared_ptr<PowerModelPlugin> m_Model{ nullptr };

        uint32_t m_SampleStart;
        uint32_t m_SampleEnd;
        uint32_t m_TraceCount;
        uint32_t m_TraceStep;
        uint32_t m_SampleFilterCount{};
    };

}
