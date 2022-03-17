/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "distinguisher.hpp"

namespace metrisca {

    class PearsonDistinguisher : public DistinguisherPlugin {
    public:

        virtual Result<std::vector<std::pair<uint32_t, Matrix<double>>>, Error> Distinguish() override;

    };

}