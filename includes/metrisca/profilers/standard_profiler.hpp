/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "profiler.hpp"

namespace metrisca {

    class StandardProfiler : public ProfilerPlugin {
    public:
        virtual Result<Matrix<double>, int> Profile() override;

    private:
        Result<void, int> ProfileSBOX(Matrix<double>& out) const;
        void ProfilerSBOXFixed(Matrix<double>& out) const;
    };

}
