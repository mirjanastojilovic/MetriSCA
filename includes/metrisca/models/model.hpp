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

namespace metrisca {

    class PowerModelPlugin : public Plugin {
    public:
        PowerModelPlugin()
            : Plugin(PluginType::PowerModel)
        {}
        virtual ~PowerModelPlugin() = default;

        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<Matrix<int32_t>, Error> Model() = 0;

        void SetByteIndex(uint32_t byteIndex);

        inline void SetDataset(std::shared_ptr<TraceDataset> dataset)
        {
            m_Dataset = std::move(dataset);
        }

    protected:
        std::shared_ptr<TraceDataset> m_Dataset;
        uint32_t m_ByteIndex{};
    };

}
