/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/core/plugin.hpp"

#include <vector>
#include <cstdint>

namespace metrisca {

    class DistinguisherPlugin : public Plugin {
    public:
        DistinguisherPlugin()
            : Plugin(PluginType::Distinguisher)
        {}

        virtual ~DistinguisherPlugin() = default;

        virtual Result<void, int> Init(const ArgumentList& args) override;

        virtual Result<std::vector<std::pair<uint32_t, Matrix<double>>>, int> Distinguish() = 0;

    protected:
        std::vector<std::pair<uint32_t, Matrix<double>>> InitializeResultMatrices();

    protected:
        std::shared_ptr<TraceDataset> m_Dataset{ nullptr };
        std::shared_ptr<PowerModelPlugin> m_PowerModel{ nullptr };
        uint32_t m_SampleStart{};
        uint32_t m_SampleCount{};
        uint32_t m_TraceCount{};
        uint32_t m_TraceStep{};
    };

}
