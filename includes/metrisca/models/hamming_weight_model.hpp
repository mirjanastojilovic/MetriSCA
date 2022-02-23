/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "model.hpp"

namespace metrisca {

    class HammingWeightModel : public PowerModelPlugin {
    public:
        virtual Result<Matrix<int32_t>, int> Model() override;
    };

}
