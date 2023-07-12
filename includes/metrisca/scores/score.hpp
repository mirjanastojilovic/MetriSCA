/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/core/plugin.hpp"

#include <memory>
#include <stdexcept>
#include <array>
#include <vector>

namespace metrisca {

    class ScorePlugin : public Plugin {
    public:
        ScorePlugin()
            : Plugin(PluginType::Score)
        {}

        virtual ~ScorePlugin() = default;	

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<std::vector<std::pair<uint32_t, std::vector<std::array<double, 256>>>>, Error> ComputeScores() = 0;

    protected:
        std::shared_ptr<TraceDataset> m_Dataset;

        uint32_t m_SampleStart{};
        uint32_t m_SampleCount{};
        uint32_t m_TraceCount{};
        uint32_t m_TraceStep{};
    };

}

