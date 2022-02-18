/**
 * MetriSCA - A side-channel analysis library
 * Copyright 2021, School of Computer and Communication Sciences, EPFL.
 *
 * All rights reserved. Use of this source code is governed by a
 * BSD-style license that can be found in the LICENSE.md file.
 */

#ifndef _FILELOADER_HPP
#define _FILELOADER_HPP

#include "forward.hpp"

#include <functional>
#include <string>

namespace metrisca {

    /**
     * Represent a function that can load a file containing traces.
     * The arguments are defined in order as follows:
     * 1) An instance of a dataset builder. This builder must be filled with the
     *    trace data before the end of the function.
     *    Note that this function should not call the Build method of the builder,
     *    this is done by the application.
     * 2) The path to the file to load. This path is not sanitized. This task
     *    is left to the function.
     * The return value of this function is an element of the error enumeration.
     */
    typedef std::function<int(TraceDatasetBuilder&, const std::string&)> LoaderFunction;

}

#endif