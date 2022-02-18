/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _VERSION_HPP
#define _VERSION_HPP

#define STR_IMPL_(x) #x
#define STR(x) STR_IMPL_(x)

#define METRISCA_MAJOR    1
#define METRISCA_MINOR    0
#define METRISCA_REVISION 0

#define METRISCA_VERSION STR(METRISCA_MAJOR) "." STR(METRISCA_MINOR) "." STR(METRISCA_REVISION)

#endif