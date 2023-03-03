/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metric.hpp"
#include "metrisca/core/result.hpp"

#include <vector>
#include <memory>

namespace metrisca {

    class KeyEnumerationMetric : public MetricPlugin {
    public:

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<void, Error> Compute() override;

    private:
        struct KeyByteDescriptor {
            KeyByteDescriptor() {}

            KeyByteDescriptor(std::shared_ptr<PowerModelPlugin> model, uint32_t sampleStart, uint32_t sampleEnd)
                : Model(std::move(model))
                , SampleStart(sampleStart)
                , SampleEnd(sampleEnd)
            {}

            std::shared_ptr<PowerModelPlugin> Model{ nullptr };
            uint32_t SampleStart = 0;
            uint32_t SampleEnd = 0;
        };

        std::vector<KeyByteDescriptor> m_KeyByteDescriptors{};
        std::shared_ptr<TraceDataset> m_Dataset{ nullptr };
        uint32_t m_TraceCount{};
        uint32_t m_TraceStep{};
        uint32_t m_KeyCount{};
    };

}
