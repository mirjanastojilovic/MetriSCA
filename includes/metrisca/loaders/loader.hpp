/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/core/plugin.hpp"

#include <string>

namespace metrisca {

    class LoaderPlugin : public Plugin {
    public:
        LoaderPlugin()
            : Plugin(PluginType::Loader)
        {}
        virtual ~LoaderPlugin() = default;

        virtual Result<void, int> Init(const ArgumentList& args) override
        {
            return {};
        }

        virtual Result<void, int> Load(TraceDatasetBuilder& builder) = 0;
    };

}
