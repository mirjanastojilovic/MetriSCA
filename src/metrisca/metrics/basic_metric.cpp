/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#include "metrisca/metrics/basic_metric.hpp"

#include "metrisca/distinguishers/distinguisher.hpp"
#include "metrisca/core/trace_dataset.hpp"

namespace metrisca {

    Result<void, int> BasicMetricPlugin::Init(const ArgumentList& args)
    {
        auto base_result = MetricPlugin::Init(args);
        if(base_result.IsError())
            return base_result;

        auto dataset = args.GetDataset(ARG_NAME_DATASET);
        auto distinguisher = args.GetString(ARG_NAME_DISTINGUISHER);

        if(!dataset.has_value())
            return SCA_MISSING_ARGUMENT;

        if(!distinguisher.has_value())
            return SCA_MISSING_ARGUMENT;

        m_Dataset = dataset.value();
        
        auto distinguisher_or_error = PluginFactory::The().ConstructAs<DistinguisherPlugin>(PluginType::Distinguisher, distinguisher.value(), args);
        if(distinguisher_or_error.IsError())
            return distinguisher_or_error.Error();

        m_Distinguisher = distinguisher_or_error.Value();

        return {};
    }
}