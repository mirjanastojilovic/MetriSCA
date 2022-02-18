/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _METRISCA_HPP
#define _METRISCA_HPP

#include "version.hpp"

#include "app/cli_application.hpp"
#include "app/cli_view.hpp"
#include "app/cli_model.hpp"

#include "core/application.hpp"
#include "core/matrix.hpp"
#include "core/model.hpp"
#include "core/distinguisher.hpp"
#include "core/profiler.hpp"
#include "core/fileloader.hpp"
#include "core/metric.hpp"
#include "core/errors.hpp"
#include "core/csv_writer.hpp"
#include "core/trace_dataset.hpp"

#include "utils/numerics.hpp"
#include "utils/crypto.hpp"
#include "utils/math.hpp"

#endif