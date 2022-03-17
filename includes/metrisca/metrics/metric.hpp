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

    class MetricPlugin : public Plugin {
    public:
        MetricPlugin()
            : Plugin(PluginType::Metric)
        {}
        virtual ~MetricPlugin() = default;

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<void, Error> Compute() = 0;

    protected:
        std::string m_OutputFile{};
    };

}
