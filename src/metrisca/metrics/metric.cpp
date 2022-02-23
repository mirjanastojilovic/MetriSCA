/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/metric.hpp"

namespace metrisca {

    Result<void, Error> MetricPlugin::Init(const ArgumentList& args)
    {
        auto output_file = args.GetString(ARG_NAME_OUTPUT_FILE);

        if(!output_file.has_value())
            return Error::MISSING_ARGUMENT;

        m_OutputFile = output_file.value();

        return {};
    }

}