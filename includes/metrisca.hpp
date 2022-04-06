/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _METRISCA_HPP
#define _METRISCA_HPP

#include "metrisca/version.hpp"

#include "metrisca/core/assert.hpp"
#include "metrisca/core/plugin.hpp"
#include "metrisca/core/matrix.hpp"
#include "metrisca/core/errors.hpp"
#include "metrisca/core/csv_writer.hpp"
#include "metrisca/core/trace_dataset.hpp"
#include "metrisca/core/arg_list.hpp"
#include "metrisca/core/result.hpp"
#include "metrisca/core/logger.hpp"

#include "metrisca/distinguishers/distinguisher.hpp"
#include "metrisca/loaders/loader.hpp"
#include "metrisca/metrics/metric.hpp"
#include "metrisca/models/model.hpp"
#include "metrisca/profilers/profiler.hpp"

#include "metrisca/utils/math.hpp"
#include "metrisca/utils/crypto.hpp"
#include "metrisca/utils/numerics.hpp"

#endif