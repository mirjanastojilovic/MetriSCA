/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include "metrisca/core/result.hpp"
#include "metrisca/core/errors.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/models/model.hpp"

#include <vector>
#include <memory>

namespace metrisca
{
    /**
     * @brief Result of a template attack, indexed by stepIdx and keyByteIdx.
     */
    typedef std::vector<std::vector<std::array<double, 256>>> TemplateAttackResult;

    /**
     * @brief Run a template attack on the given datasets.
     * 
     * @param profilingDataset The profiling dataset.
     * @param attackDataset The attack dataset.
     * @param traceCount Number of traces to use for last steps.
     * @param traceStep Number of traces per steps.
     * @param sampleStart First sample to be considered.
     * @param sampleEnd Last sample to be considered.
     * @param sampleFilterCount Number of poi.
     */
    Result<TemplateAttackResult, Error> runTemplateAttack(
        std::shared_ptr<TraceDataset> profilingDataset,
        std::shared_ptr<TraceDataset> attackDataset,
        std::shared_ptr<PowerModelPlugin> powerModel,
        size_t traceCount,
        size_t traceStep,
        size_t sampleStart,
        size_t sampleEnd,
        size_t sampleFilterCount       
    );
}
