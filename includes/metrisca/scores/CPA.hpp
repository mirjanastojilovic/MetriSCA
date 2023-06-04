/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/core/plugin.hpp"
#include "metrisca/scores/score.hpp"
#include "metrisca/distinguishers/distinguisher.hpp"

#include <memory>
#include <array>
#include <vector>

namespace metrisca
{
    class CPAPlugin : public ScorePlugin
    {
    public:
        virtual Result<void, Error> Init(const ArgumentList& args) override;

        virtual Result<std::vector<std::pair<uint32_t, std::vector<std::array<double, 256>>>>, Error> ComputeScores() override;

    private:
        std::shared_ptr<DistinguisherPlugin> m_Distinguisher;
    };
}
