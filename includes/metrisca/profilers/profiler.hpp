/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/forward.hpp"
#include "metrisca/core/plugin.hpp"

namespace metrisca {

    class ProfilerPlugin : public Plugin {
    public:
        ProfilerPlugin()
            : Plugin(PluginType::Profiler)
        {}
        virtual ~ProfilerPlugin() = default;

        virtual Result<void, int> Init(const ArgumentList& args) override;

        virtual Result<Matrix<double>, int> Profile() = 0;

    protected:
        std::shared_ptr<TraceDataset> m_Dataset{ nullptr };
        uint8_t m_KnownKey{};
        uint32_t m_ByteIndex{};
    };

}
