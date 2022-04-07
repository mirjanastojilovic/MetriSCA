/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#pragma once

#include <cassert>

namespace metrisca {

#if defined(DEBUG)
#define METRISCA_ASSERT(check) { assert(check); }
#define METRISCA_ASSERT_NOT_REACHED() { assert(false && "Non-reachable code was executed."); }
#else
#define METRISCA_ASSERT(check)
#define METRISCA_ASSERT_NOT_REACHED()
#endif

}